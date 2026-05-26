cmake_minimum_required(VERSION 3.16)
project(TC06_PlatformGuard LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc06: WINDOWS_PLATFORM is never defined — this is a macOS/Linux project.
# Windows-specific code paths are permanently dead.
# Expected: HIGH confidence on WINDOWS_PLATFORM block.

add_executable(tc06_app src/main.cpp)
target_compile_definitions(tc06_app PRIVATE UNIX_PLATFORM=1)
