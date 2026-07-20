cmake_minimum_required(VERSION 3.31)

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
        NAME EADataStructures
        GITHUB_REPOSITORY eyalamirmusic/cpp_data_structures
        GIT_TAG main)

# eacp proper. Added here rather than inside examples/EACP because the tests want
# it too: eacp-core carries <eacp/Core/Utils/Environment.h>, which is how anything
# in this repository reads or writes a process environment variable. That is not a
# nicety - std::getenv is deprecated by Microsoft's CRT, and setting a variable has
# no portable spelling at all (_putenv_s against setenv), so every call site that
# did it by hand carried an #ifdef and a suppression. eacp answers both.
#
# The heavy half is gated on whether the app is actually being built. With
# EACP_BUILD_GRAPHICS OFF this compiles Core, SIMD and Network and stops - no
# Graphics, GPU, WebView, Camera or Video - so the engine-only test loop
# (-DPUREDOOM_BUILD_EACP_EXAMPLE=OFF, which is also what CI builds) stays a
# CPU-only build that needs no GPU. CPM dedupes by NAME, so the app's own
# requirements are expressed here and examples/EACP just links the targets.
CPMAddPackage(
        NAME eacp
        GITHUB_REPOSITORY eyalamirmusic/eacp
        GIT_TAG main
        OPTIONS
        "EACP_BUILD_GRAPHICS ${PUREDOOM_BUILD_EACP_EXAMPLE}"
        "EACP_BUILD_WEBVIEW OFF")

# eacp guards GCC's STB_GNU_UNIQUE emission with add_compile_options(-fno-gnu-unique),
# keyed on the CXX compiler being GCC - but add_compile_options lands on every
# language, including the OBJCXX its Apple platform files compile, and OBJCXX on
# macOS is always Apple Clang, which rejects the GCC-only flag as an unknown
# argument. A macOS GCC build (the second-compiler gate CLAUDE.md asks for) then
# fails inside eacp-core before compiling any engine code. Until eacp scopes the
# flag itself (gap log item 10), rewrite it across eacp's directory tree to apply
# to C++ only: the same flag on every GCC-compiled TU, none on the clang .mm ones.
# A target snapshots its directory's COMPILE_OPTIONS when it is created, so the
# rewrite has to visit the targets themselves, not the directory properties.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    function(puredoom_scope_gnu_unique_flag dir)
        get_directory_property(targets DIRECTORY ${dir} BUILDSYSTEM_TARGETS)
        foreach (target ${targets})
            get_target_property(options ${target} COMPILE_OPTIONS)
            if (options)
                list(TRANSFORM options REPLACE "^-fno-gnu-unique$"
                        "$<$<COMPILE_LANGUAGE:CXX>:-fno-gnu-unique>")
                set_target_properties(${target}
                        PROPERTIES COMPILE_OPTIONS "${options}")
            endif ()
        endforeach ()
        get_directory_property(subdirs DIRECTORY ${dir} SUBDIRECTORIES)
        foreach (subdir ${subdirs})
            puredoom_scope_gnu_unique_flag(${subdir})
        endforeach ()
    endfunction()
    puredoom_scope_gnu_unique_flag(${eacp_SOURCE_DIR})
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
