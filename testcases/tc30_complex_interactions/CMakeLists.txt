cmake_minimum_required(VERSION 3.16)
project(TC30_ComplexInteractions LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc30: Complex mix — multiple patterns in one larger file.
# LIVE: ENABLE_TLS=1, ENABLE_METRICS=1
# DEAD: ENABLE_RDMA (never defined), ENABLE_ZERO_COPY (never defined)
# DEAD (else): DISABLE_POOLING always defined → pool else branch dead.
# Expected: HIGH × 3 (RDMA block, ZERO_COPY block, pool-else block).

add_executable(tc30_app src/main.cpp)
target_compile_definitions(tc30_app PRIVATE
    ENABLE_TLS=1
    ENABLE_METRICS=1
    DISABLE_POOLING=1
)
