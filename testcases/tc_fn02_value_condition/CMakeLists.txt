cmake_minimum_required(VERSION 3.16)
project(TC_FN02_ValueCondition LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# FAILURE CASE — FALSE NEGATIVE  (missed dead code)
#
# LOG_LEVEL is defined with value 0 (logging disabled).
# Source uses value-dependent conditions: #if LOG_LEVEL > 0
#
# Phase 1 extracts the macro NAME from -DLOG_LEVEL=0 and records
# LOG_LEVEL as "defined" for this target.  Phase 4 checks only whether
# the macro NAME is present in the defines set — it does NOT evaluate
# the numeric value.  So #if LOG_LEVEL > 0 blocks are treated as
# potentially live (LOG_LEVEL IS defined) and never reported.
#
# Expected (correct) result : 2 HIGH findings (both #if blocks are dead)
# Actual tool result        : 0 findings  (value-blindness in Phase 4)
# Limitation exposed        : correlator does not evaluate macro values,
#                             only macro presence/absence

add_executable(tc_fn02_app src/main.cpp)
target_compile_definitions(tc_fn02_app PRIVATE LOG_LEVEL=0)
