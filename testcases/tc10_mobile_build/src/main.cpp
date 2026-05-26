#include <cstdio>

static void init_display(int dpi) {
    std::printf("[desktop] display init @ %d DPI\n", dpi);
}

#ifdef MOBILE_BUILD
// Touch-input and high-DPI scaling for mobile — MOBILE_BUILD never defined.
static void init_touch_input() {
    std::printf("[mobile] touch input initialised\n");
}
static void set_ui_scale(float factor) {
    std::printf("[mobile] UI scale: %.2f\n", factor);
}
static bool is_retina_display() {
    return true;
}
#endif  // MOBILE_BUILD

int main() {
    init_display(SCREEN_DPI);
    return 0;
}
