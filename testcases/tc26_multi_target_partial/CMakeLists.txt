cmake_minimum_required(VERSION 3.16)
project(TC26_MultiTargetPartial LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc26: Two targets compile the SAME file (shared.cpp).
# TARGET_SERVER defines SERVER_PUSH; TARGET_CLIENT does not.
# SERVER_PUSH code is dead from client's perspective.
# Expected: HIGH confidence on SERVER_PUSH block (absent in all client configs).

add_library(tc26_shared STATIC src/shared.cpp)

add_executable(tc26_server src/server_main.cpp)
target_link_libraries(tc26_server PRIVATE tc26_shared)
target_compile_definitions(tc26_server PRIVATE SERVER_BUILD=1)
# Recompile shared.cpp with server defines by building it per-target:
target_sources(tc26_server PRIVATE src/shared.cpp)
target_compile_definitions(tc26_server PRIVATE SERVER_PUSH=1)

add_executable(tc26_client src/client_main.cpp)
target_link_libraries(tc26_client PRIVATE tc26_shared)
target_compile_definitions(tc26_client PRIVATE CLIENT_BUILD=1)
# Client does NOT define SERVER_PUSH.
