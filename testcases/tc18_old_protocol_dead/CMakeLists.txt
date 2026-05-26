cmake_minimum_required(VERSION 3.16)
project(TC18_OldProtocolDead LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc18: PROTOCOL_V2 is always ON — the #else (V1 framing) is never compiled.
# Expected: HIGH confidence on the #else branch of PROTOCOL_V2.

add_executable(tc18_app src/main.cpp)
target_compile_definitions(tc18_app PRIVATE PROTOCOL_V2=1)
