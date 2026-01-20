#ifndef UTHREAD_H
#define UTHREAD_H

#include <functional>

namespace uthread {
    // Initialize the library (must call this first!)
    void init();

    // Create a new thread that runs 'func'
    void create(void (*func)());

    // Voluntarily give up the CPU to the next thread
    void yield();

    // (Internal helper, usually not needed by user directly)
    void exit();
}

#endif
