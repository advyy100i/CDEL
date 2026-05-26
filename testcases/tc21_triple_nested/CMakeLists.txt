cmake_minimum_required(VERSION 3.16)
project(TC21_TripleNested LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc21: Three-level nesting — EXPERIMENTAL is never defined.
# EXPERIMENTAL > EXPERIMENTAL_GPU > EXPERIMENTAL_GPU_TENSOR all dead.
# Expected: HIGH × 3 (outer + two inner blocks).

add_executable(tc21_app src/main.cpp)
target_compile_definitions(tc21_app PRIVATE STABLE_BUILD=1)
