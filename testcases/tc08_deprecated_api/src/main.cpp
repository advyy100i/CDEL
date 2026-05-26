#include <cstdio>
#include <string>

// V2 API — current, always compiled.
struct Response {
    int    status;
    std::string body;
};

static Response fetch_v2(const std::string& url) {
    (void)url;
    return {200, "ok"};
}

#ifdef DEPRECATED_V1_API
// V1 API shim — retained for backward compatibility until 2023-Q2.
// DEPRECATED_V1_API is not defined in any target: shim is dead code.
struct LegacyResponse {
    char* data;
    int   len;
};

static LegacyResponse* fetch_v1(const char* url) {
    (void)url;
    std::printf("[V1] legacy fetch stub\n");
    return nullptr;
}

static void free_legacy_response(LegacyResponse* r) {
    (void)r;
}
#endif  // DEPRECATED_V1_API

int main() {
    auto r = fetch_v2("https://example.com/api");
    std::printf("status: %d\n", r.status);
    return 0;
}
