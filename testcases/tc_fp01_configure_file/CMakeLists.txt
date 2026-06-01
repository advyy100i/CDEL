cmake_minimum_required(VERSION 3.16)
project(TC_FP01_ConfigureFile LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# FAILURE CASE — FALSE POSITIVE
#
# HAVE_NETWORK_STACK is defined via configure_file(), which writes it into a
# generated header (config.h).  It is NOT passed as a -D flag on the compiler
# command line, so it never appears in compile_commands.json.
#
# Phase 1 only reads compile_commands.json for -D flags.  It will NOT see
# HAVE_NETWORK_STACK as a define for this target, so Phase 4 will report the
# #ifdef HAVE_NETWORK_STACK block as HIGH-confidence dead code.
#
# Expected (correct) result : 0 findings  (the block IS live)
# Actual tool result        : 1 HIGH false positive on HAVE_NETWORK_STACK
# Limitation exposed        : Phase 1 blind to configure_file()-defined macros

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
)

add_executable(tc_fp01_app src/main.cpp)
target_include_directories(tc_fp01_app PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
