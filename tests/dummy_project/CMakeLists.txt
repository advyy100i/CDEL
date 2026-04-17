cmake_minimum_required(VERSION 3.16)
project(DeadFeatureDummy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_EXPERIMENTAL "Enable experimental code paths" ON)
option(ENABLE_LEGACY "Enable legacy compatibility code" OFF)

add_library(dummy_core STATIC
  src/feature.cpp
)

target_include_directories(dummy_core PUBLIC src)
target_compile_definitions(dummy_core PRIVATE CORE_BUILD=1)

if(ENABLE_EXPERIMENTAL)
  target_compile_definitions(dummy_core PRIVATE EXPERIMENTAL_FEATURE=1)
endif()

if(ENABLE_LEGACY)
  target_compile_definitions(dummy_core PRIVATE LEGACY_MODE=1)
endif()

add_executable(dummy_app
  src/main.cpp
)

target_link_libraries(dummy_app PRIVATE dummy_core)
target_compile_definitions(dummy_app PRIVATE APP_BUILD=1)
