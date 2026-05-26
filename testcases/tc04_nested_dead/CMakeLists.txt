cmake_minimum_required(VERSION 3.16)
project(TC04_NestedDead LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc04: HARDWARE_ACCEL is never defined in any target.
# The outer #ifdef and every inner #ifdef inside it are all dead.
# Expected: HIGH confidence findings on HARDWARE_ACCEL + inner blocks (GPU_BACKEND, FPGA_BACKEND).

add_executable(tc04_app src/main.cpp)
target_compile_definitions(tc04_app PRIVATE SOFTWARE_ONLY=1)
