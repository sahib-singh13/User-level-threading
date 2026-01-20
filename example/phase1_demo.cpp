#include "../include/uthread.h"
#include <iostream>

void task1() {
    for (int i = 0; i < 3; ++i) {
        std::cout << "Task 1 - Step " << i << "\n";
        uthread::yield(); // Be nice and let Task 2 run
    }
}

void task2() {
    for (int i = 0; i < 3; ++i) {
        std::cout << "Task 2 - Step " << i << "\n";
        uthread::yield(); // Be nice and let Task 1 run
    }
}

int main() {
    std::cout << "[Main] Init library...\n";
    uthread::init();

    std::cout << "[Main] Creating threads...\n";
    uthread::create(task1);
    uthread::create(task2);

    std::cout << "[Main] Starting loop (main yields to let threads run)...\n";
    
    // In a real scheduler, we might have a 'uthread_join_all()'
    // For now, we manually yield enough times to let them finish.
    for (int i = 0; i < 10; i++) {
        uthread::yield();
    }

    std::cout << "[Main] Done.\n";
    return 0;
}
