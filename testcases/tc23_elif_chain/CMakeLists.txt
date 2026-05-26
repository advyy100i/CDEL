cmake_minimum_required(VERSION 3.16)
project(TC23_ElifChain LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc23: Tests #elif handling.
# LOG_LEVEL_DEBUG defined, LOG_LEVEL_TRACE never defined, LOG_LEVEL_VERBOSE never defined.
# The #elif TRACE and #elif VERBOSE arms are dead.
# Expected: HIGH × 2 (TRACE and VERBOSE elif branches).

add_executable(tc23_app src/main.cpp)
target_compile_definitions(tc23_app PRIVATE LOG_LEVEL_DEBUG=1)
