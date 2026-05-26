cmake_minimum_required(VERSION 3.16)
project(TC16_DebugBranchDead LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc16: RELEASE_BUILD is always ON — the #else (debug path) is never compiled.
# Expected: HIGH confidence on the #else branch of RELEASE_BUILD.

add_executable(tc16_app src/main.cpp)
target_compile_definitions(tc16_app PRIVATE RELEASE_BUILD=1)
