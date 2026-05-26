#include <cstdio>

// LIVE: FEATURE_A is defined in the target.
#ifdef FEATURE_A
static void feature_a() {
    std::printf("Feature A active.\n");
}
#endif

// DEAD: DEAD_FEATURE_X is never defined.
#ifdef DEAD_FEATURE_X
static void dead_feature_x() {
    std::printf("Dead feature X — never compiled.\n");
}
static int dead_helper_x(int v) { return v * 2; }
#endif

// LIVE: FEATURE_B is defined in the target.
#ifdef FEATURE_B
static void feature_b() {
    std::printf("Feature B active.\n");
}
#endif

// DEAD: DEAD_FEATURE_Y is never defined.
#ifdef DEAD_FEATURE_Y
static void dead_feature_y() {
    std::printf("Dead feature Y — also never compiled.\n");
}
#endif

int main() {
#ifdef FEATURE_A
    feature_a();
#endif
#ifdef FEATURE_B
    feature_b();
#endif
    return 0;
}
