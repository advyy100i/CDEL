cmake_minimum_required(VERSION 3.16)
project(TC12_HotpatchOff LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc12: ENABLE_HOT_PATCH option defaults OFF and is never wired.
# Live-patching infrastructure in source is permanently dead.
# Expected: HIGH confidence on ENABLE_HOT_PATCH block.

option(ENABLE_HOT_PATCH "Allow live function patching at runtime" OFF)

add_executable(tc12_app src/main.cpp)
target_compile_definitions(tc12_app PRIVATE STATIC_DISPATCH=1)
