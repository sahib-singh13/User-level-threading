#include "../include/uthread.h"
#include <ucontext.h>
#include <vector>
#include <deque>
#include <iostream>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <cassert>
#include <algorithm>
#include <random>

// Configuration
const int STACK_SIZE = 64 * 1024;

enum class ThreadState { READY, RUNNING, BLOCKED, FINISHED };

struct TCB {
    int id;
    ucontext_t context;
    std::vector<char> stack;
    ThreadState state;
    void (*func)();
    int priority; // Kept for future use, simpler logic for now

    TCB(int tid, void (*f)(), int prio) 
        : id(tid), state(ThreadState::READY), func(f), priority(prio) {
        stack.resize(STACK_SIZE);
    }
};

// ---------------------------------------------------------
// Worker Structure (Per-Core Data)
// ---------------------------------------------------------
struct Worker {
    int id;
    std::thread thread_obj; 
    std::deque<std::shared_ptr<TCB>> ready_queue;
    std::mutex queue_lock;
    std::shared_ptr<TCB> current_thread;
    
    // We need a separate context for the scheduler loop itself
    ucontext_t sched_context; 

    Worker(int worker_id) : id(worker_id) {}
};

// Global State
static std::vector<std::unique_ptr<Worker>> workers;
static std::atomic<int> next_tid{0};
static std::atomic<bool> system_running{true};

// Thread Local Pointer: "Who am I?"
static thread_local Worker* my_worker = nullptr;

// ---------------------------------------------------------
// Scheduler Logic
// ---------------------------------------------------------

static void schedule() {
    while (system_running) {
        std::shared_ptr<TCB> next_task = nullptr;

        // 1. Try local queue
        {
            std::lock_guard<std::mutex> lock(my_worker->queue_lock);
            if (!my_worker->ready_queue.empty()) {
                next_task = my_worker->ready_queue.front();
                my_worker->ready_queue.pop_front();
            }
        }

        // 2. Work Stealing (if local empty)
        if (!next_task && workers.size() > 1) {
            // Pick random victim
            int victim_id = rand() % workers.size();
            if (victim_id != my_worker->id) {
                Worker* victim = workers[victim_id].get();
                if (victim->queue_lock.try_lock()) {
                    if (!victim->ready_queue.empty()) {
                        // Steal from back!
                        next_task = victim->ready_queue.back();
                        victim->ready_queue.pop_back();
                        // std::cout << "[Worker " << my_worker->id << "] Stole task " << next_task->id << " from Worker " << victim->id << "\n";
                    }
                    victim->queue_lock.unlock();
                }
            }
        }

        if (next_task) {
            // Context Switch to Task
            my_worker->current_thread = next_task;
            next_task->state = ThreadState::RUNNING;
            
            // We swap from the "Scheduler Context" to the "Task Context"
            swapcontext(&my_worker->sched_context, &next_task->context);
            
            // ... We return here when the task yields or finishes ...
            my_worker->current_thread = nullptr;
        } else {
            // No work found? Sleep briefly to save CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

// Trampoline for User Threads
static void thread_start_wrapper() {
    if (my_worker->current_thread && my_worker->current_thread->func) {
        my_worker->current_thread->func();
    }
    
    // Task Finished
    my_worker->current_thread->state = ThreadState::FINISHED;
    // Swap back to scheduler
    setcontext(&my_worker->sched_context);
}

// Trampoline for Worker Threads (OS threads)
static void worker_entry_point(int worker_id) {
    my_worker = workers[worker_id].get();
    // std::cout << "[System] Worker " << worker_id << " online.\n";
    
    schedule(); // Enter loop
}

// ---------------------------------------------------------
// Public API
// ---------------------------------------------------------

namespace uthread {
    void init(int num_cores) {
        if (num_cores <= 0) num_cores = 4; // Default to 4 workers

        // Create Worker 0 (for the main thread)
        workers.push_back(std::make_unique<Worker>(0));
        my_worker = workers[0].get(); // Main thread claims Worker 0

        // Create other workers
        for (int i = 1; i < num_cores; ++i) {
            workers.push_back(std::make_unique<Worker>(i));
            // Launch std::thread
            workers[i]->thread_obj = std::thread(worker_entry_point, i);
        }
    }

    void create(void (*func)(), int priority) {
        auto tcb = std::make_shared<TCB>(next_tid++, func, priority);
        
        // Init Context
        getcontext(&tcb->context);
        tcb->context.uc_stack.ss_sp = tcb->stack.data();
        tcb->context.uc_stack.ss_size = STACK_SIZE;
        tcb->context.uc_link = nullptr;
        makecontext(&tcb->context, thread_start_wrapper, 0);

        // Push to LOCAL queue
        {
            std::lock_guard<std::mutex> lock(my_worker->queue_lock);
            my_worker->ready_queue.push_back(tcb);
        }
    }

    void yield() {
        auto tcb = my_worker->current_thread;
        
        // Push current task back to queue
        {
            std::lock_guard<std::mutex> lock(my_worker->queue_lock);
            tcb->state = ThreadState::READY;
            my_worker->ready_queue.push_back(tcb);
        }

        // Return to scheduler
        // Note: We use swapcontext to save current state and jump to scheduler
        swapcontext(&tcb->context, &my_worker->sched_context);
    }
    
    // Simple helper to join all threads (spin until empty)
    // In a real system we'd use condition variables
    void run_scheduler_loop() {
        schedule();
    }
    
    void exit() {
        // Just switch back to scheduler; don't push to queue
        setcontext(&my_worker->sched_context);
    }
    
    // Stub Mutex (Simplified for Phase 4 demo)
    // Since we changed the backend, the old Mutex needs updates for multi-core safety.
    // For this specific phase demo, we will rely on atomic/implicit logic or ignore Mutex for a moment.
   void Mutex::lock() { 
    // No-op for now
}

void Mutex::unlock() { 
    // No-op for now
}
}
