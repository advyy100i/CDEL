cmake_minimum_required(VERSION 3.16)
project(TC08_DeprecatedApi LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc08: DEPRECATED_V1_API is never defined — V1 was removed in 2023.
# All remaining source guards for the deprecated API are dead.
# Expected: HIGH confidence on DEPRECATED_V1_API block.

add_executable(tc08_app src/main.cpp)
target_compile_definitions(tc08_app PRIVATE API_VERSION=2)
