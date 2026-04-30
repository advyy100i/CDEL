#pragma once
#include <vector>
#include <atomic>

class Connection;

class Scheduler {
public:
    Scheduler();
    ~Scheduler();

    void addConnection(Connection* conn);
    void removeConnection(Connection* conn);
    void run();
    void shutdown();
    bool isRunning() const { return running_.load(); }

private:
    std::vector<Connection*> connections_;
    std::atomic<bool>        running_{false};

    void pollOnce();

#ifdef ENABLE_FIBER_SCHEDULING
    // Cooperative fiber scheduler prototype — ENABLE_FIBER_SCHEDULING never defined
    struct Fiber;
    std::vector<Fiber*> fibers_;
    void  runWithFibers();
    Fiber* spawnFiber(void (*fn)(void*), void* arg);
    void  yieldFiber();
#endif
};
