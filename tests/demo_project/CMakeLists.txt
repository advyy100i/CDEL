cmake_minimum_required(VERSION 3.16)
project(PulsarNet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Feature flags ─────────────────────────────────────────────────────────────
option(ENABLE_TLS           "Enable TLS/SSL transport encryption"       ON)
option(ENABLE_HTTP2         "Enable HTTP/2 protocol framing"            OFF)
option(ENABLE_METRICS       "Enable runtime performance metrics"         ON)
option(ENABLE_LEGACY_COMPAT "Enable legacy 4-byte frame header compat"  OFF)
option(ENABLE_DEBUG_LOGGING "Enable verbose debug output"               OFF)

# Helper macro — propagates all active feature flags to a target
macro(pulsar_apply_feature_flags TGT)
    if(ENABLE_TLS)
        target_compile_definitions(${TGT} PRIVATE ENABLE_TLS=1)
    endif()
    if(ENABLE_HTTP2)
        target_compile_definitions(${TGT} PRIVATE ENABLE_HTTP2=1)
    endif()
    if(ENABLE_METRICS)
        target_compile_definitions(${TGT} PRIVATE ENABLE_METRICS=1)
    endif()
    if(ENABLE_LEGACY_COMPAT)
        target_compile_definitions(${TGT} PRIVATE ENABLE_LEGACY_COMPAT=1)
    endif()
    if(ENABLE_DEBUG_LOGGING)
        target_compile_definitions(${TGT} PRIVATE ENABLE_DEBUG_LOGGING=1)
    endif()
endmacro()

# ── Core library ──────────────────────────────────────────────────────────────
# NOTE: ENABLE_IPX_PROTOCOL, ENABLE_RDMA, ENABLE_ZERO_COPY_TLS,
#       ENABLE_FIBER_SCHEDULING, ENABLE_TRACING are referenced in source
#       but intentionally NEVER passed here — they are permanently dead.
add_library(pulsar_core STATIC
    src/connection.cpp
    src/protocol.cpp
    src/metrics.cpp
    src/crypto.cpp
    src/scheduler.cpp
)

target_include_directories(pulsar_core PUBLIC include)
target_compile_definitions(pulsar_core PRIVATE PULSAR_CORE_BUILD=1)
pulsar_apply_feature_flags(pulsar_core)

# ── Server executable ─────────────────────────────────────────────────────────
add_executable(pulsar_server src/server_main.cpp)
target_link_libraries(pulsar_server PRIVATE pulsar_core)
target_compile_definitions(pulsar_server PRIVATE PULSAR_SERVER_BUILD=1)
pulsar_apply_feature_flags(pulsar_server)

# ── Client executable ─────────────────────────────────────────────────────────
add_executable(pulsar_client src/client_main.cpp)
target_link_libraries(pulsar_client PRIVATE pulsar_core)
target_compile_definitions(pulsar_client PRIVATE PULSAR_CLIENT_BUILD=1)
pulsar_apply_feature_flags(pulsar_client)
