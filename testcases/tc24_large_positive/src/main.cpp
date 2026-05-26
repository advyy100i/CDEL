#include <cstdio>

// All 8 macros are defined — zero dead blocks.

#ifdef FEATURE_AUTH
static bool authenticate(const char* token) {
    (void)token;
    return true;
}
#endif

#ifdef FEATURE_CACHE
static const char* cache_get(const char* key) {
    (void)key;
    return nullptr;
}
#endif

#ifdef FEATURE_RATE_LIMIT
static bool check_rate_limit(int user_id) {
    (void)user_id;
    return true;
}
#endif

#ifdef FEATURE_RETRY
static int retry_count() { return 3; }
#endif

#ifdef FEATURE_TIMEOUT
static int timeout_ms() { return 5000; }
#endif

#ifdef FEATURE_COMPRESSION
static int compress_level() { return 6; }
#endif

#ifdef FEATURE_LOGGING
static void log_request(const char* path) {
    std::printf("[log] %s\n", path);
}
#endif

#ifdef FEATURE_METRICS
static void record_latency(long ms) {
    std::printf("[metric] latency=%ld ms\n", ms);
}
#endif

int main() {
#ifdef FEATURE_AUTH
    authenticate("token-abc");
#endif
#ifdef FEATURE_CACHE
    cache_get("key");
#endif
#ifdef FEATURE_RATE_LIMIT
    check_rate_limit(1);
#endif
#ifdef FEATURE_LOGGING
    log_request("/api/v2/data");
#endif
#ifdef FEATURE_METRICS
    record_latency(42);
#endif
    return 0;
}
