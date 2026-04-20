cmake_minimum_required(VERSION 3.16)
project(IfdefMapper C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Locate LLVM & Clang ───────────────────────────────────────────────────────
# Prefer an explicit LLVM_DIR env var, then llvm-config on PATH.

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
  message(STATUS "Using LLVM CMake dir from llvm-config: ${LLVM_DIR}")
endif()

find_package(LLVM REQUIRED CONFIG HINTS ${LLVM_DIR})

# Derive Clang CMake dir from LLVM dir (sibling directory).
if(NOT Clang_DIR)
  get_filename_component(_llvm_cmake_parent "${LLVM_DIR}" DIRECTORY)
  set(Clang_DIR "${_llvm_cmake_parent}/clang")
endif()

find_package(Clang REQUIRED CONFIG HINTS ${Clang_DIR})

message(STATUS "LLVM version : ${LLVM_VERSION}")
message(STATUS "Clang dir    : ${Clang_DIR}")

# ── Compiler flags from LLVM ──────────────────────────────────────────────────
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}")

# ── Plugin target ─────────────────────────────────────────────────────────────
add_library(IfdefMapper MODULE IfdefMapper.cpp)

target_include_directories(IfdefMapper SYSTEM PRIVATE
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS}
)

target_compile_definitions(IfdefMapper PRIVATE
  ${LLVM_DEFINITIONS_LIST}
)

# Clang plugins must NOT link Clang/LLVM statically on macOS;
# the hosting clang process exports those symbols at runtime.
if(APPLE)
  target_link_options(IfdefMapper PRIVATE -undefined dynamic_lookup)
elseif(UNIX)
  target_link_options(IfdefMapper PRIVATE -Wl,--allow-shlib-undefined)
endif()

# No lib prefix (plugin is loaded by name, not linked as a library).
set_target_properties(IfdefMapper PROPERTIES PREFIX "")
