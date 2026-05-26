cmake_minimum_required(VERSION 3.16)
project(TC19_BoundsCheckDead LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc19: BOUNDS_CHECKING always ON → unchecked (fast) path never compiled.
# Expected: HIGH confidence on the #else branch of BOUNDS_CHECKING.

add_executable(tc19_app src/main.cpp)
target_compile_definitions(tc19_app PRIVATE BOUNDS_CHECKING=1)
