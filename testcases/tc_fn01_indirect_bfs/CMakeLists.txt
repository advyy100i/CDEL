cmake_minimum_required(VERSION 3.16)
project(TC_FN01_IndirectBFS LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# FAILURE CASE — FALSE NEGATIVE  (missed dead code)
#
# ENABLE_LEGACY_PATH=1 is defined, so legacy_handler() IS compiled and appears
# in the IR.  At runtime it is never called — main() always dispatches to
# dispatch[0] (modern_handler).
#
# Phase 3 BFS should detect this as MEDIUM confidence (compiled but
# unreachable).  However, because legacy_handler's address is stored in the
# dispatch[] array, BFS sees an indirect CallBase in main() and conservatively
# marks ALL address-taken functions — including legacy_handler — as reachable.
#
# Expected (correct) result : 1 MEDIUM finding on legacy_handler
# Actual tool result        : 0 findings  (BFS over-approximation)
# Limitation exposed        : conservative indirect-call handling in Phase 3

add_executable(tc_fn01_app src/main.cpp)
target_compile_definitions(tc_fn01_app PRIVATE ENABLE_LEGACY_PATH=1)
