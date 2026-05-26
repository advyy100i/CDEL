#include <cstdio>
#include <cstdlib>
#include <vector>

static int safe_get(const std::vector<int>& v, size_t idx) {
#ifdef BOUNDS_CHECKING
    if (idx >= v.size()) {
        std::fprintf(stderr, "index %zu out of bounds (size %zu)\n", idx, v.size());
        std::exit(1);
    }
    return v[idx];
#else
    // Fast unchecked path — BOUNDS_CHECKING is always ON so this is dead.
    return v.data()[idx];
#endif
}

int main() {
    std::vector<int> v = {10, 20, 30};
    std::printf("v[1] = %d\n", safe_get(v, 1));
    return 0;
}
