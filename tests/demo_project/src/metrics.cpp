#include "pulsar/metrics.h"
#include <cstdio>

// ── Active metrics implementation ─────────────────────────────────────────────

#ifdef ENABLE_METRICS
#include <atomic>

namespace metrics {

static std::atomic<uint64_t> g_bytes_sent{0};
static std::atomic<uint64_t> g_bytes_recv{0};
static std::atomic<uint64_t> g_conn_total{0};
static std::atomic<uint64_t> g_conn_ok{0};

void recordSend(size_t bytes)      { g_bytes_sent += bytes; }
void recordRecv(size_t bytes)      { g_bytes_recv += bytes; }

void recordConnection(bool success) {
    g_conn_total++;
    if (success) g_conn_ok++;
}

uint64_t getSentBytes()       { return g_bytes_sent.load(); }
uint64_t getRecvBytes()       { return g_bytes_recv.load(); }
uint64_t getConnectionCount() { return g_conn_total.load(); }

void reset() {
    g_bytes_sent  = 0;
    g_bytes_recv  = 0;
    g_conn_total  = 0;
    g_conn_ok     = 0;
}

void printReport() {
    std::printf("=== Pulsar Metrics ===\n");
    std::printf("  Sent:        %llu bytes\n",
                (unsigned long long)g_bytes_sent.load());
    std::printf("  Recv:        %llu bytes\n",
                (unsigned long long)g_bytes_recv.load());
    std::printf("  Connections: %llu total, %llu ok\n",
                (unsigned long long)g_conn_total.load(),
                (unsigned long long)g_conn_ok.load());
}

// ── Distributed trace sink — permanently dead ─────────────────────────────────
#ifdef ENABLE_TRACING
// ENABLE_TRACING was removed from the build in sprint 44 and never re-added.
// It is intentionally absent from all CMake target_compile_definitions.

static constexpr size_t TRACE_BUF_CAP = 4096;
static char   g_trace_buf[TRACE_BUF_CAP];
static size_t g_trace_pos = 0;

void traceEvent(const char* name, uint64_t timestamp_us) {
    int n = std::snprintf(g_trace_buf + g_trace_pos,
                          TRACE_BUF_CAP - g_trace_pos,
                          "{\"n\":\"%s\",\"ts\":%llu},",
                          name, (unsigned long long)timestamp_us);
    if (n > 0 && static_cast<size_t>(n) < TRACE_BUF_CAP - g_trace_pos)
        g_trace_pos += static_cast<size_t>(n);
}

void flushTraceBuffer() {
    if (g_trace_pos == 0) return;
    g_trace_buf[g_trace_pos - 1] = '\0';  // strip trailing comma
    std::printf("[TRACE] [%s]\n", g_trace_buf);
    g_trace_pos = 0;
}
#endif  // ENABLE_TRACING

}  // namespace metrics

// ── Stub implementation when metrics are disabled ─────────────────────────────
#else

namespace metrics {
void     recordSend(size_t)        {}
void     recordRecv(size_t)        {}
void     recordConnection(bool)    {}
uint64_t getSentBytes()            { return 0; }
uint64_t getRecvBytes()            { return 0; }
uint64_t getConnectionCount()      { return 0; }
void     reset()                   {}
void     printReport()             {}
}  // namespace metrics

#endif  // ENABLE_METRICS
