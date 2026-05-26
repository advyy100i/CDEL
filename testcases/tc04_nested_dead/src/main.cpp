#include <cstdio>

// Software path — always compiled.
float dot_product_sw(const float* a, const float* b, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; ++i)
        sum += a[i] * b[i];
    return sum;
}

// HARDWARE_ACCEL is never defined in any CMake target.
// The outer block and all nested blocks inside are permanently dead.
#ifdef HARDWARE_ACCEL

#ifdef GPU_BACKEND
// GPU path — nested inside the already-dead HARDWARE_ACCEL block.
float dot_product_gpu(const float* a, const float* b, int n) {
    (void)a; (void)b; (void)n;
    std::printf("[GPU] dot product stub\n");
    return 0.0f;
}
#endif  // GPU_BACKEND

#ifdef FPGA_BACKEND
// FPGA path — also nested inside HARDWARE_ACCEL.
float dot_product_fpga(const float* a, const float* b, int n) {
    (void)a; (void)b; (void)n;
    std::printf("[FPGA] dot product stub\n");
    return 0.0f;
}
#endif  // FPGA_BACKEND

// Dispatcher inside dead outer block.
float dot_product_hw(const float* a, const float* b, int n) {
#ifdef GPU_BACKEND
    return dot_product_gpu(a, b, n);
#else
    return dot_product_fpga(a, b, n);
#endif
}

#endif  // HARDWARE_ACCEL

int main() {
    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {4.0f, 5.0f, 6.0f};
    std::printf("dot = %.1f\n", dot_product_sw(a, b, 3));
    return 0;
}
