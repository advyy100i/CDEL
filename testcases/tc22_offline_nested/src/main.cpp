#include <cstdio>
#include <string>

static std::string fetch_online(const std::string& key) {
    std::printf("[online] fetching '%s'\n", key.c_str());
    return "value";
}

#ifdef OFFLINE_MODE
// Offline fallback — OFFLINE_MODE never defined.

static std::string read_local_store(const std::string& key) {
    std::printf("[offline] reading local store: %s\n", key.c_str());
    return "";
}

#ifdef OFFLINE_CACHE
// In-memory LRU cache layer — nested inside dead OFFLINE_MODE.
struct CacheEntry { std::string key, value; };
static CacheEntry cache_table[64];
static int cache_count = 0;

static bool cache_get(const std::string& key, std::string& out) {
    for (int i = 0; i < cache_count; ++i)
        if (cache_table[i].key == key) { out = cache_table[i].value; return true; }
    return false;
}

#ifdef OFFLINE_COMPRESS
// LZ4 compression for cache entries — triple-nested, also dead.
static std::string compress_value(const std::string& v) {
    std::printf("[lz4] compressing %zu bytes\n", v.size());
    return v;  // stub
}
static std::string decompress_value(const std::string& v) {
    return v;  // stub
}
#endif  // OFFLINE_COMPRESS

#endif  // OFFLINE_CACHE

#endif  // OFFLINE_MODE

int main() {
    auto v = fetch_online("config.max_retries");
    std::printf("value: %s\n", v.c_str());
    return 0;
}
