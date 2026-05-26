cmake_minimum_required(VERSION 3.16)
project(TC20_UnicodeAsciiDead LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc20: UNICODE_SUPPORT always ON → ASCII-only narrow path is never compiled.
# Expected: HIGH confidence on the #else branch of UNICODE_SUPPORT.

add_executable(tc20_app src/main.cpp)
target_compile_definitions(tc20_app PRIVATE UNICODE_SUPPORT=1)
