#include <cstdio>

static void modern_init() {
    std::printf("[modern] initialising\n");
}

#ifndef DISABLE_COMPAT
// Backward-compat shim — DISABLE_COMPAT is always ON, so this
// #ifndef block is permanently dead.
static void compat_init_v1() {
    std::printf("[compat] v1 init shim\n");
}

static void compat_teardown_v1() {
    std::printf("[compat] v1 teardown shim\n");
}
#endif  // !DISABLE_COMPAT

int main() {
    modern_init();
    return 0;
}
