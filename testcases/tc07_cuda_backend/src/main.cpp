#include <cstdio>
#include <vector>

// CPU fallback — always compiled.
static void matrix_multiply_cpu(const float* A, const float* B, float* C, int N) {
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < N; ++k)
                sum += A[i * N + k] * B[k * N + j];
            C[i * N + j] = sum;
        }
}

#ifdef ENABLE_CUDA
// GPU kernel launch — ENABLE_CUDA is never defined in any CMake target.
// This entire block is permanently dead: no CUDA hardware in CI.
static void matrix_multiply_gpu(const float* A, const float* B, float* C, int N) {
    (void)A; (void)B; (void)C; (void)N;
    std::printf("[CUDA] cudaMalloc + kernel launch stub\n");
}

static bool cuda_device_available() {
    // Would call cudaGetDeviceCount
    return false;
}
#endif  // ENABLE_CUDA

int main() {
    const int N = 4;
    std::vector<float> A(N * N, 1.0f), B(N * N, 2.0f), C(N * N, 0.0f);
    matrix_multiply_cpu(A.data(), B.data(), C.data(), N);
    std::printf("C[0][0] = %.1f\n", C[0]);
    return 0;
}
