#include "pulsar/connection.h"
#include "pulsar/protocol.h"
#include "pulsar/metrics.h"

#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    std::string host = (argc > 1) ? argv[1] : "127.0.0.1";
    int         port = (argc > 2) ? std::atoi(argv[2]) : 8080;

#ifdef PULSAR_CLIENT_BUILD
    std::printf("[CLIENT] Pulsar client v1.0\n");
#endif

#ifdef ENABLE_DEBUG_LOGGING
    std::printf("[DEBUG] connecting to %s:%d\n", host.c_str(), port);
#endif

    Connection conn(host, port);
    if (!conn.connect()) {
        std::printf("[CLIENT] connection failed\n");
        return 1;
    }

#ifdef ENABLE_LEGACY_COMPAT
    std::printf("[CLIENT] legacy compatibility mode active\n");
#endif

    // Send a PING frame (RFC 7540 §6.7)
    Frame ping{};
    ping.type      = FrameType::PING;
    ping.flags     = 0;
    ping.stream_id = 0;
    ping.payload   = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

    std::vector<uint8_t> wire;
    Protocol::serializeFrame(ping, wire);
    conn.send(wire.data(), wire.size());

    // Read PING ACK
    uint8_t resp[64] = {};
    size_t  n        = 0;
    if (conn.recv(resp, sizeof(resp), n) && n >= 9) {
        Frame ack = Protocol::parseFrame(resp, n);
        (void)ack;
#ifdef ENABLE_DEBUG_LOGGING
        std::printf("[DEBUG] ACK type=%d flags=0x%02X\n",
                    static_cast<int>(ack.type), ack.flags);
#endif
    }

    conn.disconnect();

#ifdef ENABLE_METRICS
    std::printf("[CLIENT] session stats:\n");
    metrics::printReport();
#endif

    return 0;
}
