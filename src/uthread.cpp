#include "../include/uthread.h"
#include <ucontext.h>
#include <vector>
#include <deque>
#include <iostream>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <cassert>
#include <algorithm>
#include <random>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

// Configuration
const int STACK_SIZE = 64 * 1024;
const int MAX_EVENTS = 64; // Max IO events to process per tick

enum class ThreadState { READY, RUNNING, BLOCKED, FINISHED };

struct TCB {
    int id;
    ucontext_t context;
    std::vector<char> stack;
    ThreadState state;
    void (*func)();
    
    TCB(int tid, void (*f)()) : id(tid), state(ThreadState::READY), func(f) {
        stack.resize(STACK_SIZE);
    }
};

struct Worker {
    int id;
    std::thread thread_obj; 
    std::deque<std::shared_ptr<TCB>> ready_queue;
    std::mutex queue_lock;
    std::shared_ptr<TCB> current_thread;
    ucontext_t sched_context; 

    Worker(int worker_id) : id(worker_id) {}
};

// Global State
static std::vector<std::unique_ptr<Worker>> workers;
static std::atomic<int> next_tid{0};
static std::atomic<bool> system_running{true};
static thread_local Worker* my_worker = nullptr;

// ---------------------------------------------------------
// NEW: IO Poller (Global)
// ---------------------------------------------------------
static int global_epoll_fd = -1;
static std::mutex poll_lock;
static std::unordered_map<int, std::shared_ptr<TCB>> fd_to_thread;

static void init_poller() {
    global_epoll_fd = epoll_create1(0);
    if (global_epoll_fd == -1) {
        perror("epoll_create1");
        exit(1);
    }
}

// ---------------------------------------------------------
// Scheduler Logic
// ---------------------------------------------------------

static void check_io_events() {
    if (poll_lock.try_lock()) {
        struct epoll_event events[MAX_EVENTS];
        int n = epoll_wait(global_epoll_fd, events, MAX_EVENTS, 0);
        
        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            auto it = fd_to_thread.find(fd);
            if (it != fd_to_thread.end()) {
                auto tcb = it->second;
                fd_to_thread.erase(it); // Remove from waiting map
                
                epoll_ctl(global_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);

                tcb->state = ThreadState::READY;
                {
                    std::lock_guard<std::mutex> qlock(my_worker->queue_lock);
                    my_worker->ready_queue.push_back(tcb);
                }
            }
        }
        poll_lock.unlock();
    }
}

static void schedule() {
    while (system_running) {
        std::shared_ptr<TCB> next_task = nullptr;

        check_io_events();

        // Try local queue
        {
            std::lock_guard<std::mutex> lock(my_worker->queue_lock);
            if (!my_worker->ready_queue.empty()) {
                next_task = my_worker->ready_queue.front();
                my_worker->ready_queue.pop_front();
            }
        }

        // Work Stealing
        if (!next_task && workers.size() > 1) {
            int victim_id = rand() % workers.size();
            if (victim_id != my_worker->id) {
                Worker* victim = workers[victim_id].get();
                if (victim->queue_lock.try_lock()) {
                    if (!victim->ready_queue.empty()) {
                        next_task = victim->ready_queue.back();
                        victim->ready_queue.pop_back();
                    }
                    victim->queue_lock.unlock();
                }
            }
        }

        if (next_task) {
            my_worker->current_thread = next_task;
            next_task->state = ThreadState::RUNNING;
            swapcontext(&my_worker->sched_context, &next_task->context);
            my_worker->current_thread = nullptr;
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}

static void thread_start_wrapper() {
    if (my_worker->current_thread && my_worker->current_thread->func) {
        my_worker->current_thread->func();
    }
    my_worker->current_thread->state = ThreadState::FINISHED;
    setcontext(&my_worker->sched_context);
}

static void worker_entry_point(int worker_id) {
    my_worker = workers[worker_id].get();
    schedule();
}

// ---------------------------------------------------------
// Public API Implementation
// ---------------------------------------------------------

namespace uthread {
    void init(int num_cores) {
        if (num_cores <= 0) num_cores = 4;
        init_poller();

        workers.push_back(std::make_unique<Worker>(0));
        my_worker = workers[0].get();

        for (int i = 1; i < num_cores; ++i) {
            workers.push_back(std::make_unique<Worker>(i));
            workers[i]->thread_obj = std::thread(worker_entry_point, i);
        }
    }

    void create(void (*func)(), int priority) {
        (void)priority;
        auto tcb = std::make_shared<TCB>(next_tid++, func);
        getcontext(&tcb->context);
        tcb->context.uc_stack.ss_sp = tcb->stack.data();
        tcb->context.uc_stack.ss_size = STACK_SIZE;
        tcb->context.uc_link = nullptr;
        makecontext(&tcb->context, thread_start_wrapper, 0);

        std::lock_guard<std::mutex> lock(my_worker->queue_lock);
        my_worker->ready_queue.push_back(tcb);
    }

    void yield() {
        auto tcb = my_worker->current_thread;
        {
            std::lock_guard<std::mutex> lock(my_worker->queue_lock);
            tcb->state = ThreadState::READY;
            my_worker->ready_queue.push_back(tcb);
        }
        swapcontext(&tcb->context, &my_worker->sched_context);
    }
    
    void run_scheduler_loop() {
        schedule(); // The main thread helps run tasks here.
        
        // --- NEW: When we return here, system_running is false. ---
        // We must wait for the other workers to finish to prevent the crash.
        for (auto& worker : workers) {
            if (worker->thread_obj.joinable()) {
                worker->thread_obj.join();
            }
        }
    }
    
    void exit() {
        setcontext(&my_worker->sched_context);
    }

    // Mutex stubs
    void Mutex::lock() {}
    void Mutex::unlock() {}

    // Async IO Implementation
    int socket_read(int fd, char* buf, size_t len) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (!(flags & O_NONBLOCK)) {
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }

        while (true) {
            ssize_t n = read(fd, buf, len);

            if (n >= 0) return n; 
            if (errno != EAGAIN && errno != EWOULDBLOCK) return -1;

            // Block and wait for Epoll
            {
                std::lock_guard<std::mutex> lock(poll_lock);
                fd_to_thread[fd] = my_worker->current_thread;
                
                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLONESHOT;
                ev.data.fd = fd;
                epoll_ctl(global_epoll_fd, EPOLL_CTL_ADD, fd, &ev);
            }

            my_worker->current_thread->state = ThreadState::BLOCKED;
            swapcontext(&my_worker->current_thread->context, &my_worker->sched_context);
        }
    }
    
    void shutdown() {
      system_running=false;
      }
}
