#pragma once
#include <cstdint>
#include <cstddef>

namespace metrics {

void     recordSend(size_t bytes);
void     recordRecv(size_t bytes);
void     recordConnection(bool success);
void     printReport();
void     reset();

uint64_t getSentBytes();
uint64_t getRecvBytes();
uint64_t getConnectionCount();

#ifdef ENABLE_TRACING
// Distributed trace sink — ENABLE_TRACING never defined in any CMake target
void traceEvent(const char* name, uint64_t timestamp_us);
void flushTraceBuffer();
#endif

}  // namespace metrics
