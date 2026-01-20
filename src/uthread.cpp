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

// Configuration
const int STACK_SIZE = 64 * 1024;
const int TIME_QUANTUM_USEC = 10000; // 10ms

// Thread States
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

// Global Scheduler State
static std::deque<std::shared_ptr<TCB>> ready_queue;
static std::shared_ptr<TCB> current_thread = nullptr;
static std::shared_ptr<TCB> main_thread = nullptr;
static int next_tid = 0;

// ---------------------------------------------------------
// Signal Safety (The "Kernel Lock")
// ---------------------------------------------------------

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

// ---------------------------------------------------------
// Scheduler Core
// ---------------------------------------------------------

static void schedule() {
    // Note: Signals must be BLOCKED when entering here
    if (ready_queue.empty()) {
        // If current is running, keep running. If blocked/finished, switch to main.
        if (current_thread->state == ThreadState::RUNNING) {
            return; 
        }
        // Switch to main if we have nothing else
        if (current_thread != main_thread) {
            swapcontext(&current_thread->context, &main_thread->context);
        }
        return; 
    }

    auto next = ready_queue.front();
    ready_queue.pop_front();

    auto prev = current_thread;
    current_thread = next;
    current_thread->state = ThreadState::RUNNING;

    swapcontext(&prev->context, &current_thread->context);
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

static void thread_start_wrapper() {
    unblock_signals(); // Allow preemption inside the user function
    if (current_thread && current_thread->func) {
        current_thread->func();
    }
    block_signals();
    current_thread->state = ThreadState::FINISHED;
    schedule();
}

// ---------------------------------------------------------
// Public API
// ---------------------------------------------------------

namespace uthread {
    void init() {
        main_thread = std::make_shared<TCB>(next_tid++, nullptr);
        main_thread->state = ThreadState::RUNNING;
        current_thread = main_thread;
        setup_timer();
    }

    void create(void (*func)()) {
        auto tcb = std::make_shared<TCB>(next_tid++, func);
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
        schedule(); // Never returns
    }

    // -----------------------------------------------------
    // Mutex Implementation
    // -----------------------------------------------------

    Mutex::Mutex() : locked(false), owner(nullptr) {}

    void Mutex::lock() {
        block_signals(); // Critical section start

        if (!locked) {
            // Case 1: Mutex is free. Take it.
            locked = true;
            owner = current_thread;
            unblock_signals();
        } else {
            // Case 2: Mutex is taken.
            // 1. Mark current thread as BLOCKED
            current_thread->state = ThreadState::BLOCKED;
            
            // 2. Add to the Mutex's private waiting queue
            waiting_queue.push_back(current_thread);
            
            // 3. Switch to another thread (schedule)
            // Note: We do NOT push current_thread to ready_queue here!
            schedule();
            
            // When we return here, we have the lock.
            // (Because unlock() gave it to us)
            owner = current_thread;
            unblock_signals();
        }
    }

    void Mutex::unlock() {
        block_signals();
        
        if (waiting_queue.empty()) {
            // Case 1: No one is waiting. Just release.
            locked = false;
            owner = nullptr;
        } else {
            // Case 2: Someone is waiting. Wake them up!
            auto next_owner = waiting_queue.front();
            waiting_queue.pop_front();
            
            // Mark them as READY and put them back in the Global Scheduler
            next_owner->state = ThreadState::READY;
            ready_queue.push_back(next_owner);
            
            // Note: We keep locked=true, but effectively transfer ownership 
            // implicitly. The waking thread will set 'owner' in lock().
        }
        
        unblock_signals();
    }
}
