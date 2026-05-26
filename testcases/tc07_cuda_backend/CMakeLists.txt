cmake_minimum_required(VERSION 3.16)
project(TC07_CudaBackend LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc07: ENABLE_CUDA is never defined — no CUDA toolchain present or required.
# The GPU compute path is permanently dead.
# Expected: HIGH confidence on ENABLE_CUDA block.

add_executable(tc07_app src/main.cpp)
target_compile_definitions(tc07_app PRIVATE CPU_ONLY=1)
