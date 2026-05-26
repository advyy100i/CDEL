#include <cstdio>
#include <cstring>

#ifdef SECURITY_HARDENED
// Secure path using length-limited string operations.
// SECURITY_HARDENED is always ON — this is always compiled.
static void copy_string(char* dst, const char* src, size_t max) {
    std::strncpy(dst, src, max - 1);
    dst[max - 1] = '\0';
}
#else
// INSECURE fallback using unbounded strcpy — SECURITY_HARDENED is always ON.
// This entire #else block is permanently dead code.
static void copy_string(char* dst, const char* src, size_t /*max*/) {
    // Intentionally insecure — strcpy with no bounds check.
    std::strcpy(dst, src);
}

static void unsafe_format(char* buf, const char* fmt) {
    // Another unsafe helper — also dead.
    std::sprintf(buf, fmt);
}
#endif  // SECURITY_HARDENED

int main() {
    char buf[64];
    copy_string(buf, "hello world", sizeof(buf));
    std::printf("%s\n", buf);
    return 0;
}
