#include <cstdio>

// Active production path.
void baseline_function() {
    std::printf("Baseline logic running.\n");
}

// PROTOTYPE_API is never defined in any CMake target.
// This entire block is permanently dead — a prototype that was
// never promoted to a real feature.
#ifdef PROTOTYPE_API
void prototype_v1() {
    std::printf("Prototype v1 — not compiled in any build.\n");
}

void prototype_v2() {
    std::printf("Prototype v2 — also dead.\n");
}
#endif  // PROTOTYPE_API

int main() {
    baseline_function();
    return 0;
}
