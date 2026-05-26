cmake_minimum_required(VERSION 3.16)
project(TC25_MixedLiveDead LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc25: Same file has 2 live and 2 dead blocks.
# FEATURE_A and FEATURE_B are defined; DEAD_FEATURE_X and DEAD_FEATURE_Y are not.
# Expected: HIGH × 2 (DEAD_FEATURE_X, DEAD_FEATURE_Y); 0 for A and B.

add_executable(tc25_app src/main.cpp)
target_compile_definitions(tc25_app PRIVATE FEATURE_A=1 FEATURE_B=1)
