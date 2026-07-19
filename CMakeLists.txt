cmake_minimum_required(VERSION 3.31)

project(PureDOOM)

option(PUREDOOM_BUILD_EACP_EXAMPLE "" ON)
option(PUREDOOM_BUILD_TESTS "" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

# Nothing here is a C++20 module, but CMake scans for them anyway on any target
# built as C++20, which costs a scan pass per translation unit and - on GCC -
# breaks the build outright. The scan is what puts `-fmodules-ts` on the command
# line; that makes `__has_feature(modules)` true, and Apple's SDK then sends
# <cstring> down a Clang-only path (`#define __need_rsize_t` before <stddef.h>,
# which GCC's stddef.h does not honour), leaving `rsize_t` undeclared and
# memset_s unparseable. Turning the scan off is both the fix and free: turn it
# back on only when something here actually becomes a module.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# eacp's data structures (EA::Vector / EA::Array): header-only, INTERFACE-only,
# C++20, and pulls in nothing else - no eacp, no graphics. The engine uses them
# in its rewritten code, so it is added here, ahead of the engine, rather than
# inside examples/EACP (which configures after src/DOOM). CPM dedupes by NAME, so
# eacp's own find_package(EADataStructures) reuses this one when the app is built,
# and the tests keep linking doom-engine (plus this header-only target) alone.
include(CPM)
CPMAddPackage(
        NAME EADataStructures
        GITHUB_REPOSITORY eyalamirmusic/cpp_data_structures
        GIT_TAG main)

# The engine. Everything else links it.
add_subdirectory(src/DOOM)

if (PUREDOOM_BUILD_EACP_EXAMPLE)
    add_subdirectory(examples/EACP)
endif ()
if (PUREDOOM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif ()
