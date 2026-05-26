#include <cstdio>

static void run_app() {
    std::printf("Application running.\n");
}

#ifdef ENABLE_TELEMETRY
// Telemetry collection — ENABLE_TELEMETRY option exists but is never wired
// to target_compile_definitions. This block is permanently dead.
static void send_telemetry(const char* event, int value) {
    (void)event; (void)value;
    std::printf("[telemetry] %s = %d\n", event, value);
    // Would POST to analytics endpoint
}

static void flush_telemetry_queue() {
    std::printf("[telemetry] flushing queue...\n");
}
#endif  // ENABLE_TELEMETRY

int main() {
    run_app();
    return 0;
}
