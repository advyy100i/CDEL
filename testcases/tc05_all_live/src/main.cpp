#include <cstdio>

// Negative / control test: every macro used in source is defined in the
// CMake target. The tool should report ZERO dead features.

#ifdef FEATURE_ALPHA
static void alpha() {
    std::printf("Alpha feature active.\n");
}
#endif

#ifdef FEATURE_BETA
static void beta() {
    std::printf("Beta feature active.\n");
}
#endif

#ifdef ENABLE_LOGGING
static void log_event(const char* msg) {
    std::printf("[LOG] %s\n", msg);
}
#endif

int main() {
#ifdef FEATURE_ALPHA
    alpha();
#endif
#ifdef FEATURE_BETA
    beta();
#endif
#ifdef ENABLE_LOGGING
    log_event("startup");
#endif
    return 0;
}
