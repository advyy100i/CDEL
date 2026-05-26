cmake_minimum_required(VERSION 3.16)
project(TC11_TelemetryOff LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc11: ENABLE_TELEMETRY is a CMake option defaulting OFF.
# The option exists but is never applied to any target — dead code.
# Expected: HIGH confidence on ENABLE_TELEMETRY block.

option(ENABLE_TELEMETRY "Send anonymous usage statistics" OFF)

add_executable(tc11_app src/main.cpp)
# Intentionally NOT applying ENABLE_TELEMETRY via target_compile_definitions.
target_compile_definitions(tc11_app PRIVATE PRIVACY_FIRST=1)
