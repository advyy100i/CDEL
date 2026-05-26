cmake_minimum_required(VERSION 3.16)
project(TC01_NeverDefined LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc01: PROTOTYPE_API is referenced in source but intentionally absent from
# every target_compile_definitions call — it is a dead prototype leftover.
# Expected: HIGH confidence finding on the PROTOTYPE_API block.

add_executable(tc01_app src/main.cpp)
