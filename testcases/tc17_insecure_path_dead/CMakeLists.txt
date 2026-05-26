cmake_minimum_required(VERSION 3.16)
project(TC17_InsecurePathDead LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc17: SECURITY_HARDENED is always ON — insecure fallback path is never compiled.
# Expected: HIGH confidence on the #else branch of SECURITY_HARDENED.

add_executable(tc17_app src/main.cpp)
target_compile_definitions(tc17_app PRIVATE SECURITY_HARDENED=1)
