#include "pulsar/connection.h"
#include "pulsar/scheduler.h"
#include "pulsar/metrics.h"
#include "pulsar/protocol.h"
#include "pulsar/crypto.h"

#include <cstdlib>
#include <cstdio>
#include <vector>

static void handleClient(Connection& conn) {
    uint8_t buf[8192];
    size_t  received = 0;

    if (!conn.recv(buf, sizeof(buf), received)) {
        std::printf("[SERVER] recv failed\n");
        return;
    }

    Frame req = Protocol::parseFrame(buf, received);
    if (!Protocol::validateFrame(req)) {
        std::printf("[SERVER] invalid frame dropped\n");
        return;
    }

#ifdef ENABLE_DEBUG_LOGGING
    std::printf("[SERVER] frame type=%d stream=%u payload=%zu bytes\n",
                static_cast<int>(req.type), req.stream_id, req.payload.size());
#endif

    // Echo frame back to client
    std::vector<uint8_t> resp;
    Protocol::serializeFrame(req, resp);
    conn.send(resp.data(), resp.size());
}

int main(int argc, char* argv[]) {
    int port = (argc > 1) ? std::atoi(argv[1]) : 8080;

#ifndef ENABLE_TLS
    std::printf("[SERVER] WARNING: TLS disabled — plaintext mode\n");
#endif

#ifdef ENABLE_DEBUG_LOGGING
    std::printf("[DEBUG] pulsar_server listening on 0.0.0.0:%d\n", port);
#endif

    Scheduler sched;

    // Simulate a single accepted connection (real server would loop on accept())
    Connection conn("0.0.0.0", port);
    if (conn.connect()) {
        sched.addConnection(&conn);
        handleClient(conn);
        conn.disconnect();
    }

    sched.shutdown();

#ifdef ENABLE_METRICS
    std::printf("[SERVER] session complete — metrics:\n");
    metrics::printReport();
#endif

    return 0;
}
