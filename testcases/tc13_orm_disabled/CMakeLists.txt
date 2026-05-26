cmake_minimum_required(VERSION 3.16)
project(TC13_OrmDisabled LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# tc13: ENABLE_ORM option defaults OFF and is never wired to any target.
# Object-relational mapping layer is dead — project uses raw SQL instead.
# Expected: HIGH confidence on ENABLE_ORM block.

option(ENABLE_ORM "Enable object-relational mapping layer" OFF)

add_executable(tc13_app src/main.cpp)
target_compile_definitions(tc13_app PRIVATE RAW_SQL=1)
