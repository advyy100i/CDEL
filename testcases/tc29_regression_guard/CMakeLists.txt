cmake_minimum_required(VERSION 3.16)
project(TC29_RegressionGuard LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc29: REGRESSION_TESTS and INTERNAL_TESTING never defined in production builds.
# Test harness code embedded in production source is dead.
# Expected: HIGH × 2 (REGRESSION_TESTS, INTERNAL_TESTING blocks).

add_executable(tc29_app src/main.cpp)
target_compile_definitions(tc29_app PRIVATE PRODUCTION_BUILD=1)
