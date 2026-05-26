#include <cstdio>

static int compute_checksum(const int* data, int n) {
    int sum = 0;
    for (int i = 0; i < n; ++i) sum ^= data[i];
    return sum;
}

#ifdef REGRESSION_TESTS
// Built-in regression test suite — REGRESSION_TESTS never defined in prod.
// Dead code in all production builds.
static int test_checksum_empty() {
    return compute_checksum(nullptr, 0) == 0 ? 0 : 1;
}

static int test_checksum_single() {
    int v = 42;
    return compute_checksum(&v, 1) == 42 ? 0 : 1;
}

static void run_regression_suite() {
    int fail = 0;
    fail += test_checksum_empty();
    fail += test_checksum_single();
    std::printf("[REGRESSION] %d failures\n", fail);
}
#endif  // REGRESSION_TESTS

#ifdef INTERNAL_TESTING
// Internal testing instrumentation — also never defined in prod.
static void dump_internal_state(const char* label) {
    std::printf("[INTERNAL] state dump: %s\n", label);
}
#endif  // INTERNAL_TESTING

int main() {
    int data[] = {1, 2, 3, 4};
    std::printf("checksum: %d\n", compute_checksum(data, 4));
    return 0;
}
