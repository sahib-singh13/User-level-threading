#ifndef UTHREAD_H
#define UTHREAD_H

#include <deque>
#include <memory>

struct TCB; // Forward declaration

namespace uthread {
    void init(int num_cores = 0); // New arg
    void create(void (*func)(), int priority = 0);
    void yield();
    void exit();
    void run_scheduler_loop(); // New: Main thread becomes a worker too

    class Mutex {
    private:
        bool locked;
        std::shared_ptr<TCB> owner;
        std::deque<std::shared_ptr<TCB>> waiting_queue;

    public:
        Mutex();
        void lock();
        void unlock();
    };
}
#endif
