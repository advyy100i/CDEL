cmake_minimum_required(VERSION 3.16)
project(TC03_AlwaysOffFlag LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc03: ENABLE_LEGACY_ALGO exists as a CMake option but always defaults OFF.
# Because it is never applied via target_compile_definitions, no target sees it.
# Expected: HIGH confidence finding on the ENABLE_LEGACY_ALGO block.

option(ENABLE_LEGACY_ALGO "Enable legacy algorithm (deprecated)" OFF)

add_executable(tc03_app src/main.cpp)

# Intentionally no target_compile_definitions for ENABLE_LEGACY_ALGO.
# Even if the user passes -DENABLE_LEGACY_ALGO=ON to cmake, the
# target never receives the define unless wired up here.
target_compile_definitions(tc03_app PRIVATE APP_MODERN=1)
