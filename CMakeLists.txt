cmake_minimum_required(VERSION 3.31)

# The oldest macOS this is built for. **An app's own business, and one of the
# few things a library must not decide for it** - eacp sets its own only when it
# is the top-level project, deliberately, so anything embedding it states its
# floor here and states it before configuring eacp.
#
# Before project(), because that is when CMake fixes it.
#
# Leaving it out is not neutral, which is the trap: CMake's Darwin module makes
# CMAKE_OSX_DEPLOYMENT_TARGET an *empty* cache entry, every binary then targets
# whatever SDK is on the build machine, and **nothing anywhere reports it** - a
# deployment target that is too new produces no diagnostic, so the first symptom
# is an app refusing to launch on someone else's Mac. This port shipped that way
# until a stray deprecation warning in eacp's own source - visible here and not
# in eacp's build, because their floor is older than ours was - led back to it.
#
# 11.0 matches eacp's own number on purpose: this port links it, so a floor
# below theirs would be a floor their code does not honour. Not verified on a
# real macOS 11; what is verified is that the two agree and the build is
# warning-free.
# FORCE, and eacp's own copy of this says why: CMake's Darwin module creates
# CMAKE_OSX_DEPLOYMENT_TARGET as an empty cache entry, so a plain CACHE set is a
# no-op against it. Before project() that only bites on a *re*-configure of an
# existing build directory - which is every developer's, and none of CI's, so
# without FORCE this would look like it worked and quietly not. The guard is what
# keeps -DCMAKE_OSX_DEPLOYMENT_TARGET=... on the command line winning.
if (APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum macOS version, matching eacp's own" FORCE)
endif ()

project(PureDOOM)

option(PUREDOOM_BUILD_EACP_EXAMPLE "" ON)
option(PUREDOOM_BUILD_TESTS "" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

# C++20, and the *same* C++20 on every toolchain. Set here rather than per
# subdirectory so a target added anywhere inherits it, and so the three settings
# that make it portable travel together:
#
#   STANDARD_REQUIRED  - a compiler that cannot do C++20 is an error rather than
#                        a silent downgrade to whatever it can do. Without it
#                        CMake drops to C++17 and the failure surfaces later, as
#                        a confusing diagnostic in code that assumed C++20.
#   EXTENSIONS OFF     - asks GCC/Clang for -std=c++20 instead of their default
#                        -std=gnu++20. With extensions on, the engine compiles
#                        against a slightly different language on the GNU-driver
#                        platforms than on the MSVC ones, which is exactly the
#                        kind of difference that is invisible until it isn't.
#
# doom-engine aditionally states cxx_std_20 as a target_compile_feature, which
# is what propagates the requirement to anything linking it.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
# eacp's own find_package(EADataStructures) reuses this one, and the engine itself
# still links nothing but this header-only target.
include(CPM)

CPMAddPackage(
        NAME eacp
        GITHUB_REPOSITORY eyalamirmusic/eacp
        GIT_TAG main
        OPTIONS
        "EACP_BUILD_GRAPHICS ${PUREDOOM_BUILD_EACP_EXAMPLE}"
        "EACP_BUILD_WEBVIEW OFF")


CPMAddPackage(
        NAME NukedOPL3
        GITHUB_REPOSITORY nukeykt/Nuked-OPL3
        GIT_TAG master
        DOWNLOAD_ONLY YES)

add_library(nuked-opl3 STATIC ${NukedOPL3_SOURCE_DIR}/opl3.c)
target_include_directories(nuked-opl3 SYSTEM PUBLIC ${NukedOPL3_SOURCE_DIR})

# Upstream C, compiled by our C++ warning flags otherwise. Silenced rather than
# patched: nothing here may modify it, and its warnings are not ours to hold to
# zero.
if (MSVC)
    target_compile_options(nuked-opl3 PRIVATE /w)
else ()
    target_compile_options(nuked-opl3 PRIVATE -w)
endif ()

# The engine. Everything else links it.
add_subdirectory(src/DOOM)

if (PUREDOOM_BUILD_EACP_EXAMPLE)
    add_subdirectory(examples/EACP)
endif ()
if (PUREDOOM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif ()
