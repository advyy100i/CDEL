cmake_minimum_required(VERSION 3.16)
project(TC02_ElseBranchDead LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc02: NEW_PARSER is always ON in every target.
# The #else branch (old_parse_impl) is therefore never compiled.
# Expected: HIGH confidence finding on the #else branch.

add_executable(tc02_app src/main.cpp)
target_compile_definitions(tc02_app PRIVATE NEW_PARSER=1)
