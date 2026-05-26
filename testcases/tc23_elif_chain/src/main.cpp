#include <cstdio>
#include <cstdarg>

// Log level selection via #ifdef / #elif chain.
// LOG_LEVEL_DEBUG is defined → only the debug branch is compiled.
// LOG_LEVEL_TRACE and LOG_LEVEL_VERBOSE are never defined → their
// elif branches are dead.

#ifdef LOG_LEVEL_DEBUG
static void log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::printf("[DEBUG] ");
    std::vprintf(fmt, ap);
    std::printf("\n");
    va_end(ap);
}
#elif defined(LOG_LEVEL_TRACE)
// Trace-level logging — LOG_LEVEL_TRACE never defined; dead branch.
static void log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::printf("[TRACE] tid=%d ", 0);  // would call gettid()
    std::vprintf(fmt, ap);
    std::printf("\n");
    va_end(ap);
}
static void log_hex(const void* buf, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(buf);
    for (size_t i = 0; i < len; ++i)
        std::printf("%02x ", p[i]);
    std::printf("\n");
}
#elif defined(LOG_LEVEL_VERBOSE)
// Verbose logging — LOG_LEVEL_VERBOSE never defined; also dead.
static void log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::printf("[VERBOSE] file=%s line=%d ", __FILE__, __LINE__);
    std::vprintf(fmt, ap);
    std::printf("\n");
    va_end(ap);
}
#else
// Silent fallback
static void log(const char*, ...) {}
#endif

int main() {
    log("Application started");
    log("Processing %d items", 42);
    return 0;
}
