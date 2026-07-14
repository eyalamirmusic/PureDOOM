cmake_minimum_required(VERSION 3.31)

project(PureDOOM)

option(PUREDOOM_BUILD_EACP_EXAMPLE "" ON)
option(PUREDOOM_BUILD_SDL_EXAMPLE "" OFF)
option(PUREDOOM_BUILD_TESTS "" ON)

# Upstream's single whole-frame memcmp against a golden framebuffer. Superseded
# by Tests/, which drives the same engine far harder, but kept buildable.
option(PUREDOOM_BUILD_LEGACY_TESTS "" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

# The engine. Everything else links it.
add_subdirectory(src/DOOM)

if (PUREDOOM_BUILD_EACP_EXAMPLE)
    add_subdirectory(examples/EACP)
endif ()
if (PUREDOOM_BUILD_SDL_EXAMPLE)
    add_subdirectory(examples/SDL)
endif ()
if (PUREDOOM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif ()
if (PUREDOOM_BUILD_LEGACY_TESTS)
    enable_testing()
    add_subdirectory(examples/Tests)
endif ()
