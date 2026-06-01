#include <cstdio>

// LOG_LEVEL=0 is defined in CMake.  The tool records LOG_LEVEL as "defined"
// and therefore does NOT flag the blocks below — even though LOG_LEVEL=0
// means neither condition is ever true and both blocks are dead code.

#if LOG_LEVEL > 0
// Dead: LOG_LEVEL=0 → condition is false at compile time.
// Tool misses this because it only checks "is LOG_LEVEL defined?", not its value.
void log_debug(const char* msg) {
    std::printf("[DEBUG] %s\n", msg);
}

void log_info(const char* msg) {
    std::printf("[INFO]  %s\n", msg);
}
#endif  // LOG_LEVEL > 0

#if LOG_LEVEL > 1
// Also dead for the same reason.
void log_verbose(const char* msg) {
    std::printf("[VERB]  %s\n", msg);
}
#endif  // LOG_LEVEL > 1

int main() {
    // No logging calls reach here at runtime — both blocks are compiled out.
    std::printf("Running with LOG_LEVEL=0 (silent)\n");
    return 0;
}
