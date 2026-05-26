#include <cstdio>
#include <cmath>

// Scalar path — always compiled.
static float vector_norm_scalar(const float* v, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; ++i)
        sum += v[i] * v[i];
    return std::sqrt(sum);
}

#ifdef ENABLE_AVX512
// AVX-512 intrinsic path — ENABLE_AVX512 option never wired; dead code.
// This would require #include <immintrin.h> in a real implementation.
static float vector_norm_avx512(const float* v, int n) {
    (void)v; (void)n;
    std::printf("[AVX-512] _mm512_fmadd_ps stub\n");
    return 0.0f;
}

static void batch_transform_avx512(float* data, int n, float scale) {
    (void)data; (void)n; (void)scale;
    std::printf("[AVX-512] batch transform stub\n");
}
#endif  // ENABLE_AVX512

int main() {
    float v[] = {3.0f, 4.0f};
    std::printf("norm = %.2f\n", vector_norm_scalar(v, 2));
    return 0;
}
