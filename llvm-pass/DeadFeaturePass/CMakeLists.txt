cmake_minimum_required(VERSION 3.16)
project(DeadFeaturePass C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Locate LLVM ───────────────────────────────────────────────────────────────
if(NOT LLVM_DIR)
  find_program(LLVM_CONFIG
    NAMES llvm-config llvm-config-22 llvm-config-21 llvm-config-20 llvm-config-19 llvm-config-18 llvm-config-17
    HINTS /opt/homebrew/opt/llvm/bin
          /usr/local/opt/llvm/bin
          /usr/bin
          /usr/local/bin
  )
  if(NOT LLVM_CONFIG)
    message(FATAL_ERROR
      "llvm-config not found. Install LLVM (brew install llvm) and retry, "
      "or pass -DLLVM_DIR=<llvm-cmake-dir>.")
  endif()

  execute_process(
    COMMAND ${LLVM_CONFIG} --cmakedir
    OUTPUT_VARIABLE LLVM_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  message(STATUS "LLVM CMake dir: ${LLVM_DIR}")
endif()

find_package(LLVM REQUIRED CONFIG HINTS ${LLVM_DIR})
message(STATUS "LLVM version: ${LLVM_VERSION}")

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}")

# ── Pass target ───────────────────────────────────────────────────────────────
add_library(DeadFeaturePass MODULE DeadFeaturePass.cpp)

target_include_directories(DeadFeaturePass SYSTEM PRIVATE
  ${LLVM_INCLUDE_DIRS}
)

target_compile_definitions(DeadFeaturePass PRIVATE
  ${LLVM_DEFINITIONS_LIST}
)

# LLVM pass plugins use the same dynamic-lookup trick as Clang plugins.
if(APPLE)
  target_link_options(DeadFeaturePass PRIVATE -undefined dynamic_lookup)
elseif(UNIX)
  target_link_options(DeadFeaturePass PRIVATE -Wl,--allow-shlib-undefined)
endif()

set_target_properties(DeadFeaturePass PROPERTIES PREFIX "")
