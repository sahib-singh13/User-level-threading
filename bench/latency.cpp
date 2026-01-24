#include "../include/uthread.h"
#include <iostream>
#include <chrono>

const int SWITCHES = 1000000;

void ping() {
    for (int i = 0; i < SWITCHES; ++i) {
        uthread::yield();
    }
}

void pong() {
    for (int i = 0; i < SWITCHES; ++i) {
        uthread::yield();
    }
    // NEW: When the last thread is done, stop the scheduler!
    uthread::shutdown(); 
}

int main() {
    // 1 Core to measure pure software overhead
    uthread::init(1); 

    auto start = std::chrono::high_resolution_clock::now();

    uthread::create(ping);
    uthread::create(pong);
    
    // This will now return when pong calls shutdown()
    uthread::run_scheduler_loop();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::nano> elapsed = end - start;

    // Total switches = SWITCHES * 2
    double time_per_switch = elapsed.count() / (SWITCHES * 2);

    std::cout << "[Result] Context Switch Latency: " << time_per_switch << " ns\n";
    return 0;
}
