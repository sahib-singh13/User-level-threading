#include "../include/uthread.h"
#include <ucontext.h>
#include <vector>
#include <deque>
#include <iostream>
#include <memory>
#include <cassert>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <algorithm> // Needed for std::max_element

// Configuration
const int STACK_SIZE = 64 * 1024;
const int TIME_QUANTUM_USEC = 10000;

enum class ThreadState { READY, RUNNING, BLOCKED, FINISHED };

struct TCB {
    int id;
    ucontext_t context;
    std::vector<char> stack;
    ThreadState state;
    void (*func)();
    int priority; // NEW: Priority level

    TCB(int tid, void (*f)(), int prio) 
        : id(tid), state(ThreadState::READY), func(f), priority(prio) {
        stack.resize(STACK_SIZE);
    }
};

static std::deque<std::shared_ptr<TCB>> ready_queue;
static std::shared_ptr<TCB> current_thread = nullptr;
static std::shared_ptr<TCB> main_thread = nullptr;
static int next_tid = 0;

// Signal Safety Helpers
static void block_signals() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &mask, nullptr);
}

static void unblock_signals() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGVTALRM);
    sigprocmask(SIG_UNBLOCK, &mask, nullptr);
}

// Timer Handler
static void timer_handler(int signum) {
    (void)signum;
    uthread::yield();
}

static void setup_timer() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = timer_handler;
    sigaction(SIGVTALRM, &sa, nullptr);

    struct itimerval timer;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = TIME_QUANTUM_USEC;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = TIME_QUANTUM_USEC;
    setitimer(ITIMER_VIRTUAL, &timer, nullptr);
}

// ---------------------------------------------------------
// NEW: Priority Scheduler Logic
// ---------------------------------------------------------

static void schedule() {
    // Assumes signals BLOCKED
    if (ready_queue.empty()) {
        if (current_thread != main_thread && current_thread->state != ThreadState::RUNNING) {
            swapcontext(&current_thread->context, &main_thread->context);
        }
        return; 
    }

    // NEW: Find the thread with the HIGHEST priority
    // We use a lambda to compare priorities.
    auto it = std::max_element(ready_queue.begin(), ready_queue.end(), 
        [](const std::shared_ptr<TCB>& a, const std::shared_ptr<TCB>& b) {
            return a->priority < b->priority;
        });

    // 'it' is an iterator to the best thread. Copy it, then erase from queue.
    auto next = *it;
    ready_queue.erase(it);

    auto prev = current_thread;
    current_thread = next;
    current_thread->state = ThreadState::RUNNING;

    swapcontext(&prev->context, &current_thread->context);
}

static void thread_start_wrapper() {
    unblock_signals();
    if (current_thread && current_thread->func) {
        current_thread->func();
    }
    block_signals();
    current_thread->state = ThreadState::FINISHED;
    schedule();
}

namespace uthread {
    void init() {
        main_thread = std::make_shared<TCB>(next_tid++, nullptr, 0);
        main_thread->state = ThreadState::RUNNING;
        current_thread = main_thread;
        setup_timer();
    }

    // NEW: Added 'priority' argument
    void create(void (*func)(), int priority) {
        auto tcb = std::make_shared<TCB>(next_tid++, func, priority);
        getcontext(&tcb->context);
        tcb->context.uc_stack.ss_sp = tcb->stack.data();
        tcb->context.uc_stack.ss_size = STACK_SIZE;
        tcb->context.uc_link = nullptr;
        makecontext(&tcb->context, thread_start_wrapper, 0);

        block_signals();
        ready_queue.push_back(tcb);
        unblock_signals();
    }

    void yield() {
        block_signals();
        if (ready_queue.empty() && current_thread->state == ThreadState::RUNNING) {
            unblock_signals();
            return;
        }
        if (current_thread->state == ThreadState::RUNNING) {
            current_thread->state = ThreadState::READY;
            ready_queue.push_back(current_thread);
        }
        schedule();
        unblock_signals();
    }

    void exit() {
        block_signals();
        current_thread->state = ThreadState::FINISHED;
        schedule();
    }

    // Mutex implementation remains the same...
    Mutex::Mutex() : locked(false), owner(nullptr) {}

    void Mutex::lock() {
        block_signals();
        if (!locked) {
            locked = true;
            owner = current_thread;
            unblock_signals();
        } else {
            current_thread->state = ThreadState::BLOCKED;
            waiting_queue.push_back(current_thread);
            schedule();
            owner = current_thread;
            unblock_signals();
        }
    }

    void Mutex::unlock() {
        block_signals();
        if (!waiting_queue.empty()) {
            auto next_owner = waiting_queue.front();
            waiting_queue.pop_front();
            next_owner->state = ThreadState::READY;
            ready_queue.push_back(next_owner);
        }
        locked = false;
        owner = nullptr;
        unblock_signals();
    }
}
