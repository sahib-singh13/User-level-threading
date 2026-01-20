#include <iostream>
#include <ucontext.h>

// 64kB stack size is sufficient for this demo
#define STACK_SIZE 1024 * 64

ucontext_t main_context, thread_context;
char thread_stack[STACK_SIZE];

void thread_function() {
    std::cout << "[Thread] Entered thread_function. I am now running on my own stack!\n";
    
    std::cout << "[Thread] Swapping back to main...\n";
    // Save current state into thread_context, and restore main_context
    swapcontext(&thread_context, &main_context);
    
    std::cout << "[Thread] Back in thread_function! Finished.\n";
}

int main() {
    std::cout << "[Main] Starting context switch demo...\n";

    // 1. Get the current context (initializes the struct)
    if (getcontext(&thread_context) == -1) {
        perror("getcontext");
        return 1;
    }

    // 2. Set up the new stack for the thread
    thread_context.uc_stack.ss_sp = thread_stack;
    thread_context.uc_stack.ss_size = sizeof(thread_stack);
    thread_context.uc_link = &main_context; // Where to go when this context exits (optional if we manually swap back)

    // 3. Modify the context to run 'thread_function' when activated
    makecontext(&thread_context, (void (*)(void))thread_function, 0);

    std::cout << "[Main] Context initialized. Swapping to thread...\n";
    
    // 4. Save current execution point into main_context, switch to thread_context
    swapcontext(&main_context, &thread_context);

    std::cout << "[Main] Back in main! The thread yielded execution.\n";
    
    // Note: In a real scheduler, we would manage the lifecycle better. 
    // Here we just swapped back once.
    
    std::cout << "[Main] Demo complete.\n";
    return 0;
}
