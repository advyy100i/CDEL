cmake_minimum_required(VERSION 3.16)
project(TC09_LegacySerializer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc09: LEGACY_BINARY_FORMAT is never defined — binary format was replaced
# by JSON in 2022. The old serializer path is dead.
# Expected: HIGH confidence on LEGACY_BINARY_FORMAT block.

add_executable(tc09_app src/main.cpp)
target_compile_definitions(tc09_app PRIVATE JSON_FORMAT=1)
