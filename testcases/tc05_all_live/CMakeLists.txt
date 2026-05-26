cmake_minimum_required(VERSION 3.16)
project(TC05_AllLive LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc05: NEGATIVE / CONTROL test case.
# Every #ifdef macro in the source is defined in the target.
# The tool should report ZERO dead features.
# Expected: 0 findings.

add_executable(tc05_app src/main.cpp)
target_compile_definitions(tc05_app PRIVATE
    FEATURE_ALPHA=1
    FEATURE_BETA=1
    ENABLE_LOGGING=1
)
