#include "pulsar/scheduler.h"
#include "pulsar/connection.h"
#include "pulsar/metrics.h"

#include <algorithm>
#include <cstdio>

Scheduler::Scheduler() = default;

Scheduler::~Scheduler() {
    if (running_) shutdown();
}

void Scheduler::addConnection(Connection* conn) {
    connections_.push_back(conn);
}

void Scheduler::removeConnection(Connection* conn) {
    connections_.erase(
        std::remove(connections_.begin(), connections_.end(), conn),
        connections_.end());
}

void Scheduler::pollOnce() {
    for (Connection* c : connections_) {
        if (!c->isConnected()) continue;

        uint8_t buf[4096];
        size_t  received = 0;
        c->recv(buf, sizeof(buf), received);

#ifdef ENABLE_DEBUG_LOGGING
        if (received > 0) {
            std::printf("[SCHED] %s:%d → %zu bytes\n",
                        c->host().c_str(), c->port(), received);
        }
#endif
    }
}

void Scheduler::run() {
    running_ = true;

#ifdef ENABLE_FIBER_SCHEDULING
    // Fiber-based cooperative scheduler.
    // ENABLE_FIBER_SCHEDULING is intentionally absent from all CMake targets.
    runWithFibers();
#else
    while (running_) {
        pollOnce();

        // In a real event loop this would be epoll_wait/kevent/IOCP.
        // Stub: break immediately to avoid infinite loop in tests.
        break;
    }
#endif

#ifdef ENABLE_METRICS
    metrics::printReport();
#endif
}

void Scheduler::shutdown() {
    running_ = false;
}

// ── Fiber scheduler — permanently dead ───────────────────────────────────────

#ifdef ENABLE_FIBER_SCHEDULING
// Cooperative fiber scheduler was prototyped in branch `exp/fibers` but never
// merged. ENABLE_FIBER_SCHEDULING has never appeared in any CMakeLists.txt.

struct Scheduler::Fiber {
    void  (*fn)(void*);
    void*   arg;
    bool    done;
};

void Scheduler::runWithFibers() {
    while (running_) {
        for (Fiber* f : fibers_) {
            if (!f->done) {
                f->fn(f->arg);
            }
        }
        pollOnce();
        // Yield to OS — stub
        break;
    }
}

Scheduler::Fiber* Scheduler::spawnFiber(void (*fn)(void*), void* arg) {
    Fiber* f = new Fiber{fn, arg, false};
    fibers_.push_back(f);
    return f;
}

void Scheduler::yieldFiber() {
    // Platform-specific context switch — not implemented
}
#endif  // ENABLE_FIBER_SCHEDULING
