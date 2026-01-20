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

    // Print which thread ID is running this (requires a way to get ID, or just print generic)
    {
        std::lock_guard<std::mutex> lock(print_lock);
        // We can't easily print "Worker ID" without exposing it, but we can verify parallelism
        // by the speed or by adding a helper later.
        std::cout << "Heavy Task Finished! Found " << primes << " primes.\n";
    }
}

int main() {
    std::cout << "[Main] Init with 4 cores...\n";
    uthread::init(4);

    std::cout << "[Main] Spawning 16 heavy tasks...\n";
    for (int i = 0; i < 16; ++i) {
        uthread::create(heavy_task);
    }

    std::cout << "[Main] Helping run tasks...\n";
    uthread::run_scheduler_loop(); // Main thread helps processing

    return 0;
}
