#include <cstdio>

// Modern algorithm — always compiled.
double compute(double x) {
    return x * x + 2.0 * x + 1.0;
}

#ifdef ENABLE_LEGACY_ALGO
// Legacy algorithm from 2020 — CMake option defaults OFF and is never
// wired to any target, so this block is never compiled.
double compute_legacy(double x) {
    double result = 0.0;
    for (int i = 0; i < 1000; ++i)
        result += x / 1000.0;
    return result * result;
}
#endif  // ENABLE_LEGACY_ALGO

int main() {
    std::printf("result: %.4f\n", compute(3.0));
    return 0;
}
