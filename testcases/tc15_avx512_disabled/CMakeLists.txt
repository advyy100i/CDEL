cmake_minimum_required(VERSION 3.16)
project(TC15_AVX512Disabled LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc15: ENABLE_AVX512 option defaults OFF and is never applied.
# AVX-512 SIMD path is dead — CI machines don't support AVX-512.
# Expected: HIGH confidence on ENABLE_AVX512 block.

option(ENABLE_AVX512 "Use AVX-512 SIMD intrinsics" OFF)

add_executable(tc15_app src/main.cpp)
target_compile_definitions(tc15_app PRIVATE SCALAR_MATH=1)
