#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <cstring>

// This function runs when the timer expires
void timer_handler(int signum) {
    // Note: It's unsafe to use cout in a signal handler (not async-safe),
    // but for this simple demo, it's fine. In the real lib, we just swap context.
    const char* msg = "\n[Interrupt] Timer fired! Switching control...\n";
    write(STDOUT_FILENO, msg, strlen(msg));
}

void setup_timer() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &timer_handler;
    sigaction(SIGVTALRM, &sa, NULL);

    // Configure the timer
    struct itimerval timer;
    // Interval: how often to fire AFTER the first time (every 500ms)
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 500000; 
    // Value: how long to wait for the FIRST fire
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 500000;

    // Start the timer (ITIMER_VIRTUAL counts CPU time consumed by process)
    setitimer(ITIMER_VIRTUAL, &timer, NULL);
}

int main() {
    std::cout << "[Main] Setting up timer...\n";
    setup_timer();

    std::cout << "[Main] Entering infinite loop (usually this hangs forever)...\n";
    
    long long counter = 0;
    while(true) {
        counter++;
        if (counter % 100000000 == 0) {
            std::cout << ".";
            std::flush(std::cout);
        }
    }

    return 0;
}
