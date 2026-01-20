#include "../include/uthread.h"
#include <iostream>

int shared_counter = 0;
uthread::Mutex mutex;

void worker_safe() {
    for (int i = 0; i < 100000; ++i) {
        mutex.lock();
        shared_counter++; // Critical Section
        mutex.unlock();
    }
}

// A version that causes bugs (Race Condition)
void worker_unsafe() {
    for (int i = 0; i < 100000; ++i) {
        shared_counter++; 
        // Interrupt here causes data loss!
    }
}

int main() {
    uthread::init();

    std::cout << "[Main] Testing SAFE increment (with Mutex)...\n";
    shared_counter = 0;
    
    // Create 2 threads using the mutex
    uthread::create(worker_safe);
    uthread::create(worker_safe);

    // Let them run
    for(int i=0; i<50; i++) uthread::yield(); 
    // (In a real app we'd need a 'join' primitive, but yield loop works for demo)

    std::cout << "Safe Counter Result: " << shared_counter << " (Expected: 200000)\n";
    
    return 0;
}
