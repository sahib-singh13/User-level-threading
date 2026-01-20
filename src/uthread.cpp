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
const int TIME_QUANTUM_USEC = 10000; // 10ms quantum

enum class ThreadState { READY, RUNNING, FINISHED };

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
// Signal & Interrupt Management Helpers
// ---------------------------------------------------------

// Helper: Block SIGVTALRM (Pause the timer interrupts)
// We use this to protect "Critical Sections" (when modifying the queue)
static void block_signals() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGVTALRM);
    // SIG_BLOCK adds signals to the currently blocked set
    sigprocmask(SIG_BLOCK, &mask, nullptr);
}

// Helper: Unblock SIGVTALRM (Resume timer interrupts)
static void unblock_signals() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGVTALRM);
    // SIG_UNBLOCK removes signals from the blocked set
    sigprocmask(SIG_UNBLOCK, &mask, nullptr);
}

// The actual function called when the timer fires
static void timer_handler(int signum) {
    (void)signum; // Silence unused variable warning
    
    // Safety check: Don't yield if we are already switching or inside a critical section?
    // Actually, sigprocmask prevents this handler from running inside a critical section!
    // So if we are here, it is safe to yield.
    
    // Note: We are technically inside a signal handler context here.
    // Calling swapcontext saves the signal stack frame. 
    // When we resume, we return from yield(), then return from this handler.
    uthread::yield();
}

static void setup_timer() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; 
    sa.sa_handler = timer_handler;
    sigaction(SIGVTALRM, &sa, nullptr);

    struct itimerval timer;
    // Interval: Fire every 10ms
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = TIME_QUANTUM_USEC;
    // Initial delay: 10ms
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = TIME_QUANTUM_USEC;

    setitimer(ITIMER_VIRTUAL, &timer, nullptr);
}

// ---------------------------------------------------------
// Internal Scheduler
// ---------------------------------------------------------

static void schedule() {
    // Note: 'schedule' assumes signals are already blocked by the caller!

    if (ready_queue.empty()) {
        // If no threads, switch back to main (if not already there)
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

static void thread_start_wrapper() {
    // We entered a new thread. Signals might be blocked if we came from 'schedule'.
    // We MUST unblock them so this thread can be preempted!
    unblock_signals();

    if (current_thread && current_thread->func) {
        current_thread->func();
    }

    // Thread is done. We need to manipulate the queue (Critical Section).
    block_signals();
    current_thread->state = ThreadState::FINISHED;
    schedule();
    // (Never returns here)
}

// ---------------------------------------------------------
// Public API
// ---------------------------------------------------------
namespace uthread {

    void init() {
        main_thread = std::make_shared<TCB>(next_tid++, nullptr);
        main_thread->state = ThreadState::RUNNING;
        current_thread = main_thread;
        
        // Start the engine!
        setup_timer();
    }

    void create(void (*func)()) {
        auto tcb = std::make_shared<TCB>(next_tid++, func);

        getcontext(&tcb->context);
        tcb->context.uc_stack.ss_sp = tcb->stack.data();
        tcb->context.uc_stack.ss_size = STACK_SIZE;
        tcb->context.uc_link = nullptr;
        makecontext(&tcb->context, thread_start_wrapper, 0);

        // Critical Section: Modifying the queue
        block_signals();
        ready_queue.push_back(tcb);
        unblock_signals();
    }

    void yield() {
        // Critical Section: Modifying queue and switching
        block_signals();

        if (ready_queue.empty()) {
            unblock_signals();
            return;
        }

        // Move current to back
        if (current_thread->state == ThreadState::RUNNING) {
            current_thread->state = ThreadState::READY;
            ready_queue.push_back(current_thread);
        }

        schedule(); // Switches context

        // When we return here (resume), we are still inside the Critical Section.
        // We must unblock signals before returning to user code.
        unblock_signals();
    }

    void exit() {
        block_signals();
        current_thread->state = ThreadState::FINISHED;
        schedule();
    }
}
