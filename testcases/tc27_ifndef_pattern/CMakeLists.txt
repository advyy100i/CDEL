cmake_minimum_required(VERSION 3.16)
project(TC27_IfndefPattern LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc27: Tests #ifndef detection.
# DISABLE_COMPAT is always defined → the #ifndef DISABLE_COMPAT block is dead.
# Expected: HIGH confidence on the #ifndef DISABLE_COMPAT block.

add_executable(tc27_app src/main.cpp)
target_compile_definitions(tc27_app PRIVATE DISABLE_COMPAT=1)
