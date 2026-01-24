#include "../include/uthread.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <atomic>

std::atomic<int> tasks_remaining;
// "volatile" tells the compiler: "Do not delete this variable, even if it looks useless."
volatile int global_sink = 0; 

void crunch_numbers() {
    int count = 0;
    // INCREASED LOAD: 2 -> 500,000
    // This forces the CPU to check primality for half a million numbers.
    for (int i = 2; i < 500000; ++i) { 
        bool prime = true;
        for (int j = 2; j * j <= i; ++j) {
            if (i % j == 0) {
                prime = false;
                break;
            }
        }
        if (prime) count++;
    }
    
    // Write to volatile variable so the loop isn't deleted
    global_sink += count; 

    if (--tasks_remaining == 0) {
        uthread::shutdown();
    }
}

int main(int argc, char* argv[]) {
    int cores = 4; 
    if (argc > 1) cores = std::atoi(argv[1]);

    uthread::init(cores);

    auto start = std::chrono::high_resolution_clock::now();

    // 32 Tasks. 
    // On 1 core, they run one by one. 
    // On 4 cores, 4 run at once.
    int num_tasks = 32;
    tasks_remaining = num_tasks;

    for (int i = 0; i < num_tasks; ++i) {
        uthread::create(crunch_numbers);
    }

    uthread::run_scheduler_loop();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::cout << "[Result] Cores: " << cores << " | Time: " << elapsed.count() << "s\n";
    return 0;
}
