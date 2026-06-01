#include <cstdio>

// All three macros are absent from CMakeLists.txt.
// Baseline 2 (CMake-only) catches all three correctly — no LLVM needed.
// The full tool agrees, but provides no additional signal here.

#ifdef FUTURE_FEATURE_A
void feature_a_impl() {
    std::printf("Feature A — planned but never defined\n");
}
#endif

#ifdef FUTURE_FEATURE_B
void feature_b_impl() {
    std::printf("Feature B — planned but never defined\n");
}
#endif

#ifdef EXPERIMENTAL_UI
void show_experimental_ui() {
    std::printf("Experimental UI — dead\n");
}
#endif

int main() {
    std::printf("Running baseline-sufficient demo\n");
    return 0;
}
