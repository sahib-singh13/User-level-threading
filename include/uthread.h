#ifndef UTHREAD_H
#define UTHREAD_H

#include <deque>
#include <memory>

struct TCB; // Forward declaration

namespace uthread {
    void init();
    void create(void (*func)());
    void yield();
    void exit();

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
