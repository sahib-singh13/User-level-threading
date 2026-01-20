#include "../include/uthread.h"
#include <iostream>
#include <unistd.h>

void low_priority_task() {
    while(true) {
        std::cout << "Low Priority Task\n";
        // Slow it down slightly so we don't flood the console too fast
        for (volatile int i = 0; i < 100000; ++i); 
    }
}

void high_priority_task() {
    while(true) {
        std::cout << "!!! HIGH PRIORITY TASK !!!\n";
        for (volatile int i = 0; i < 100000; ++i);
    }
}

int main() {
    uthread::init();

    std::cout << "[Main] Creating Low Priority (0)...\n";
    uthread::create(low_priority_task, 0);

    std::cout << "[Main] Creating High Priority (10)...\n";
    uthread::create(high_priority_task, 10);

    // Let them fight!
    while(true) {} 

    return 0;
}
