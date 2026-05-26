cmake_minimum_required(VERSION 3.16)
project(TC24_LargePositive LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc24: Large negative test — 8 macros, ALL defined in the target.
# The tool should report ZERO dead features.
# Expected: 0 findings.

add_executable(tc24_app src/main.cpp)
target_compile_definitions(tc24_app PRIVATE
    FEATURE_AUTH=1
    FEATURE_CACHE=1
    FEATURE_RATE_LIMIT=1
    FEATURE_RETRY=1
    FEATURE_TIMEOUT=1
    FEATURE_COMPRESSION=1
    FEATURE_LOGGING=1
    FEATURE_METRICS=1
)
