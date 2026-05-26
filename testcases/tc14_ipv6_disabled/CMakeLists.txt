cmake_minimum_required(VERSION 3.16)
project(TC14_IPv6Disabled LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc14: ENABLE_IPV6 option defaults OFF, never wired to any target.
# IPv6 dual-stack code path is permanently dead.
# Expected: HIGH confidence on ENABLE_IPV6 block.

option(ENABLE_IPV6 "Enable IPv6 dual-stack support" OFF)

add_executable(tc14_app src/main.cpp)
target_compile_definitions(tc14_app PRIVATE IPV4_ONLY=1)
