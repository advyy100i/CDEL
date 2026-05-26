#include <cstdio>
#include <string>
#include <locale>
#include <codecvt>

#ifdef UNICODE_SUPPORT
// Wide-character / UTF-8 path — UNICODE_SUPPORT is always ON.
static std::string normalize(const std::string& input) {
    // Stub: in production, apply Unicode NFC normalization.
    return input;
}

static size_t count_codepoints(const std::string& s) {
    // Stub: actual UTF-8 codepoint counting.
    return s.size();
}
#else
// ASCII-only path — UNICODE_SUPPORT is always ON so this is dead.
static std::string normalize(const std::string& input) {
    std::string out = input;
    for (char& c : out)
        if (c > 127) c = '?';  // strip non-ASCII
    return out;
}

static size_t count_codepoints(const std::string& s) {
    return s.size();  // trivial for ASCII-only
}
#endif  // UNICODE_SUPPORT

int main() {
    std::string s = "hello";
    std::printf("'%s' → %zu codepoints\n", normalize(s).c_str(), count_codepoints(s));
    return 0;
}
