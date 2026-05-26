cmake_minimum_required(VERSION 3.16)
project(TC22_OfflineNested LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc22: OFFLINE_MODE > OFFLINE_CACHE > OFFLINE_COMPRESS — all three never defined.
# Expected: HIGH × 3.

add_executable(tc22_app src/main.cpp)
target_compile_definitions(tc22_app PRIVATE ONLINE_MODE=1)
