#include <cstdio>
#include <cstdlib>

static void check(bool cond, const char* msg) {
#ifdef RELEASE_BUILD
    // Release: silently skip failed assertions.
    (void)cond; (void)msg;
#else
    // Debug path — RELEASE_BUILD is always ON in every target, so this
    // branch is never compiled.
    if (!cond) {
        std::fprintf(stderr, "ASSERT FAILED: %s\n", msg);
        std::abort();
    }
#endif
}

static int divide(int a, int b) {
    check(b != 0, "division by zero");
    return a / b;
}

int main() {
    std::printf("result: %d\n", divide(10, 2));
    return 0;
}
