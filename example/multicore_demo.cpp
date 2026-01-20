#include "../include/uthread.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <mutex>

std::mutex print_lock; // To prevent garbled console output

void heavy_task() {
    // Calculate primes to burn CPU
    int primes = 0;
    // We do a moderately heavy calculation
    for (int i = 2; i < 50000; ++i) {
        bool is_prime = true;
        for (int j = 2; j * j <= i; ++j) {
            if (i % j == 0) {
                is_prime = false;
                break;
            }
        }
        if (is_prime) primes++;
    }

    {
        std::lock_guard<std::mutex> lock(print_lock);
        std::cout << "Heavy Task Finished! Found " << primes << " primes.\n";
    }
}

int main() {
    // 1. Initialize with 4 worker threads (simulating 4 cores)
    std::cout << "[Main] Init with 4 cores...\n";
    uthread::init(4);

    // 2. Spawn 16 heavy tasks
    // If we only had 1 core, these would run one by one.
    // With 4 cores, 4 should run at once!
    std::cout << "[Main] Spawning 16 heavy tasks...\n";
    for (int i = 0; i < 16; ++i) {
        uthread::create(heavy_task);
    }

    // 3. Main thread joins the party to help process tasks
    std::cout << "[Main] Helping run tasks...\n";
    uthread::run_scheduler_loop(); 

    return 0;
}
