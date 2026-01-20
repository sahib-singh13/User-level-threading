#include "../include/uthread.h"
#include <ucontext.h>
#include <vector>
#include <deque>
#include <iostream>
#include <memory>
#include <cassert>

// Configuration
const int STACK_SIZE = 64 * 1024; // 64kB

enum class ThreadState { READY, RUNNING, FINISHED };

// ---------------------------------------------------------
// Thread Control Block (TCB)
// ---------------------------------------------------------
struct TCB {
    int id;
    ucontext_t context;
    std::vector<char> stack; 
    ThreadState state;
    void (*func)(); // Pointer to the function this thread should run

    TCB(int tid, void (*f)()) : id(tid), state(ThreadState::READY), func(f) {
        stack.resize(STACK_SIZE);
    }
};

// ---------------------------------------------------------
// Global Scheduler State
// ---------------------------------------------------------
static std::deque<std::shared_ptr<TCB>> ready_queue;
static std::shared_ptr<TCB> current_thread = nullptr;
static std::shared_ptr<TCB> main_thread = nullptr; // Represents the original main()
static int next_tid = 0;

// ---------------------------------------------------------
// Internal Helpers
// ---------------------------------------------------------

// Switch to the next available thread
static void schedule() {
    if (ready_queue.empty()) {
        // If no threads left, we assume we return to the main scheduler loop or exit
        // For this simple implementation, we just switch back to main_thread 
        // if we aren't already there.
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

    // Debug log
    // std::cout << "[Sched] Swapping " << prev->id << " -> " << current_thread->id << "\n";

    swapcontext(&prev->context, &current_thread->context);
}

// The "Trampoline" function
static void thread_start_wrapper() {
    // 1. Execute the user's function
    if (current_thread && current_thread->func) {
        current_thread->func();
    }

    // 2. Mark as finished
    current_thread->state = ThreadState::FINISHED;
    // std::cout << "[Sched] Thread " << current_thread->id << " finished.\n";

    // 3. Schedule the next thread (never returns here)
    schedule();
}

// ---------------------------------------------------------
// Public API
// ---------------------------------------------------------
namespace uthread {

    void init() {
        // Create a TCB for the main execution thread
        // It doesn't need a stack buffer because it uses the OS stack
        main_thread = std::make_shared<TCB>(next_tid++, nullptr);
        main_thread->state = ThreadState::RUNNING;
        current_thread = main_thread;
    }

    void create(void (*func)()) {
        auto tcb = std::make_shared<TCB>(next_tid++, func);

        // Initialize ucontext
        getcontext(&tcb->context);
        
        // Set up stack
        tcb->context.uc_stack.ss_sp = tcb->stack.data();
        tcb->context.uc_stack.ss_size = STACK_SIZE;
        tcb->context.uc_link = nullptr; // We manually handle exit in wrapper

        // Set entry point to our trampoline wrapper
        makecontext(&tcb->context, thread_start_wrapper, 0);

        ready_queue.push_back(tcb);
    }

    void yield() {
        // If ready queue is empty, nothing to do
        if (ready_queue.empty()) return;

        // Move current thread to back of queue
        if (current_thread->state == ThreadState::RUNNING) {
            current_thread->state = ThreadState::READY;
            ready_queue.push_back(current_thread);
        }

        schedule();
    }

    void exit() {
        current_thread->state = ThreadState::FINISHED;
        schedule();
    }
}
