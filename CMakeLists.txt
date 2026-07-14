cmake_minimum_required(VERSION 3.31)

project(PureDOOM)

option(PUREDOOM_BUILD_EACP_EXAMPLE "" ON)
option(PUREDOOM_BUILD_TESTS "" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

# The engine. Everything else links it.
add_subdirectory(src/DOOM)

if (PUREDOOM_BUILD_EACP_EXAMPLE)
    add_subdirectory(examples/EACP)
endif ()
if (PUREDOOM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif ()
