cmake_minimum_required(VERSION 3.16)
project(TC28_VendorExtension LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc28: VENDOR_ACME_SDK is never defined — vendor SDK integration was
# replaced by an open-source library but the old guard remains.
# Expected: HIGH confidence on VENDOR_ACME_SDK block.

add_executable(tc28_app src/main.cpp)
target_compile_definitions(tc28_app PRIVATE USE_OPEN_SOURCE_LIB=1)
