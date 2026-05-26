#include <cstdio>

// Stable path — always compiled.
static float inference_stable(const float* weights, int n) {
    float s = 0.0f;
    for (int i = 0; i < n; ++i) s += weights[i];
    return s / n;
}

// EXPERIMENTAL is never defined — this outer block and all nested
// blocks inside it are permanently dead.
#ifdef EXPERIMENTAL
static float inference_experimental(const float* weights, int n) {
    // Experimental base implementation
    (void)weights; (void)n;
    std::printf("[exp] experimental inference\n");
    return 0.0f;
}

#ifdef EXPERIMENTAL_GPU
// GPU-accelerated experimental path — nested inside already-dead EXPERIMENTAL.
static float inference_gpu(const float* weights, int n) {
    (void)weights; (void)n;
    std::printf("[exp-gpu] GPU inference stub\n");
    return 0.0f;
}

#ifdef EXPERIMENTAL_GPU_TENSOR
// Tensor-core path — triple-nested inside two dead outer blocks.
static float inference_tensor_core(const float* weights, int n) {
    (void)weights; (void)n;
    std::printf("[exp-gpu-tc] tensor core inference stub\n");
    return 0.0f;
}
#endif  // EXPERIMENTAL_GPU_TENSOR

#endif  // EXPERIMENTAL_GPU

#endif  // EXPERIMENTAL

int main() {
    float w[] = {1.0f, 2.0f, 3.0f, 4.0f};
    std::printf("inference: %.2f\n", inference_stable(w, 4));
    return 0;
}
