cmake_minimum_required(VERSION 3.16)
project(TC_BL01_BaselineSufficient LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# LIMITATION CASE — BASELINE SUFFICIENT (tool adds no value)
#
# Three macros are all absent from every target_compile_definitions call.
# Baseline 2 (CMake-only, Phase 1 output) identifies all three as dead with
# 100% precision/recall — no IR analysis needed.
#
# The full 4-phase tool reaches the same conclusion but requires:
#   - Building the Clang IfdefMapper plugin (Phase 2)
#   - Building the DeadFeaturePass LLVM plugin (Phase 3)
#   - Running LTO bitcode compilation
#
# For this class of simple, single-target project the IR pipeline provides
# zero additional findings over a plain CMake-define check.
#
# Expected (correct) result : 3 HIGH findings from both Baseline 2 AND full tool
# Baseline 2 result         : 3 HIGH  (correct, fast, no LLVM required)
# Full tool result          : 3 HIGH  (correct, but expensive for same answer)
# Limitation exposed        : IR reachability adds no value when macros are
#                             simply absent from all target_compile_definitions

add_executable(tc_bl01_app src/main.cpp)
# Intentionally no target_compile_definitions — all three macros are absent
