cmake_minimum_required(VERSION 3.16)
project(TC10_MobileBuild LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc10: MOBILE_BUILD is never defined — this project targets desktop only.
# Mobile-specific UI scaling and touch-input code is dead.
# Expected: HIGH confidence on MOBILE_BUILD block.

add_executable(tc10_app src/main.cpp)
target_compile_definitions(tc10_app PRIVATE DESKTOP_BUILD=1 SCREEN_DPI=96)
