#include "../include/uthread.h"
#include <iostream>
#include <chrono>

// A "Greedy" task that never calls yield()!
// It just spins the CPU.
void greedy_task1() {
    long long counter = 0;
    while (true) {
        counter++;
        if (counter % 50000000 == 0) {
            std::cout << "[Task 1] Still running... (Count: " << counter << ")\n";
        }
        // No uthread::yield() here! 
    }
}

void greedy_task2() {
    long long counter = 0;
    while (true) {
        counter++;
        if (counter % 50000000 == 0) {
            std::cout << "[Task 2] Still running... (Count: " << counter << ")\n";
        }
        // No uthread::yield() here!
    }
}

int main() {
    std::cout << "[Main] Initializing Preemptive Scheduler...\n";
    uthread::init();

    std::cout << "[Main] Creating greedy threads...\n";
    uthread::create(greedy_task1);
    uthread::create(greedy_task2);

    // Main thread also needs to do something, or wait.
    // We'll just loop for a while to let the others run.
    while(true) {
        // Just keep the process alive so threads can run
    }

    return 0;
}
