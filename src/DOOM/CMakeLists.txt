# The DOOM engine itself.
#
# This fork owns these sources and modifies them. The app and the test suite
# both link this one library, so a change to the simulation reaches both and
# cannot go stale in one of them. It is the only way to build the engine: the
# generated single header is gone, and with it the rule that no two files may
# share a file-scope name.

# Vanilla's files are the flat ones. The subdirectories are the rewrite: a file
# moves into one the moment it stops being 1993 C and becomes C++ someone wrote,
# and the vanilla name disappears. Progress is the flat list getting shorter.
file(GLOB DOOM_VANILLA_SOURCES CONFIGURE_DEPENDS *.cpp *.h)

# Containers.h is flat but is not vanilla: it is the container vocabulary the
# eight subdirectories share, written for this refactor. Moving it across the two
# lists changes no compilation (both flag sets are a no-op on a header) - it keeps
# the lists meaning what their names say.
list(REMOVE_ITEM DOOM_VANILLA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Containers.h)

file(GLOB DOOM_REWRITTEN_SOURCES CONFIGURE_DEPENDS
        Math/*.cpp Math/*.h
        Sim/*.cpp Sim/*.h
        Thinkers/*.cpp Thinkers/*.h
        Render/*.cpp Render/*.h
        UI/*.cpp UI/*.h
        Game/*.cpp Game/*.h
        Host/*.cpp Host/*.h
        Wad/*.cpp Wad/*.h
        Engine/*.cpp Engine/*.h)
list(APPEND DOOM_REWRITTEN_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Containers.h)

add_library(doom-engine STATIC ${DOOM_VANILLA_SOURCES} ${DOOM_REWRITTEN_SOURCES})
add_library(DOOM::engine ALIAS doom-engine)

target_compile_features(doom-engine PUBLIC cxx_std_20)

# eacp's EA::Vector / EA::Array, used by the rewritten engine code. Header-only
# and INTERFACE-only; added at the root ahead of this subdirectory. PUBLIC so the
# rewritten headers that expose these types (Sim/Level.h, Math/Trig.h, ...) carry
# the include path to the app and the tests.
target_link_libraries(doom-engine PUBLIC ea_data_structures)

# eacp-core, for the platform work the engine would otherwise hand-roll per OS.
# PRIVATE: this is an implementation choice, not part of the engine's interface -
# DOOM.h stays a plain extern "C" header with no eacp type anywhere in it, and an
# embedder still overrides any of it through the doom_set_* callbacks. A consumer
# that wants eacp itself links eacp itself.
target_link_libraries(doom-engine PRIVATE eacp-core)

# The DOOM_IMPLEMENT_* gates went with the extern "C" API: Doom::Host's
# constructor (Host/Host.cpp) installs working defaults unconditionally, and an
# embedder overrides members of Doom::host() instead of pre-setting pointers.
# (Every build here defined all six, so the unconditional defaults change
# nothing. DOOM_IMPLEMENT_SOCKETS was never read by any line of source; the
# socket code gates on I_NET_ENABLED, which nothing defines.)

# Consumers include <DOOM/DOOM.h>. The engine's own files include each other by
# bare name, which resolves next to them.
target_include_directories(doom-engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Which spelling a flag takes is decided by the compiler's *driver*, not by who
# wrote the compiler. clang-cl is CMAKE_CXX_COMPILER_ID "Clang" behind an MSVC
# front end, so keying this on the compiler ID - as it was - handed GCC-style
# flags to an MSVC-style driver. That is not merely ignored: under clang-cl,
# `-Wall` is MSVC's /Wall, which clang implements as -Weverything. The engine
# reported ~44,000 warnings on Windows/ARM64 from that one substitution, nearly
# all of them -Wold-style-cast, -Wpadded and -Wunsafe-buffer-usage, and most of
# them raised inside eacp's headers rather than in any code here.
#
# CMAKE_CXX_COMPILER_FRONTEND_VARIANT is the question actually being asked: what
# command line does this thing accept? It is "MSVC" for both cl and clang-cl, and
# "GNU" for gcc, clang and Apple Clang.
if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(DOOM_WARNINGS_OFF /w)

    # /W4 is close to -Wall -Wextra but not equal to it: it also asks four
    # questions whose GCC/Clang equivalents this project has deliberately not
    # enabled. Turning those off is what makes "zero warnings" mean the same
    # thing on every toolchain, rather than a number that depends on which
    # compiler ran. Each is disabled for a stated reason, not for being noisy:
    #
    #   C4244/C4267  narrowing conversion. The GNU spelling is -Wconversion,
    #                which is in neither -Wall nor -Wextra. DOOM assigns int
    #                expressions into short and byte fields everywhere and the
    #                truncation is the behaviour the demos depend on; 121 of
    #                these are that idiom. Enabling -Wconversion everywhere is a
    #                real decision someone could make, and it should be made for
    #                all compilers at once rather than arrived at by accident on
    #                one of them.
    #   C4459/C4457  declaration hides a global / a parameter. That is -Wshadow,
    #                also in neither -Wall nor -Wextra.
    #   C4702        unreachable code. -Wunreachable-code, which Clang keeps out
    #                of -Wall/-Wextra too.
    #   C4805        mixing bool and int in one operation. No GNU equivalent at
    #                any level. The one instance of it that turned out to be
    #                worth acting on - SpriteFrame::rotate, a tri-state int being
    #                compared against `true` and `false` - has been fixed at the
    #                source rather than silenced here.
    #
    # Raising the bar is a separate, deliberate piece of work (the -Werror item
    # in REFACTOR.md's "What is left"): turn these back on together with
    # -Wconversion and -Wshadow on the GNU side, and fix what all of them find.
    set(DOOM_WARNINGS_ON /W4 /wd4244 /wd4267 /wd4459 /wd4457 /wd4702 /wd4805)
else ()
    set(DOOM_WARNINGS_OFF -w)
    set(DOOM_WARNINGS_ON -Wall -Wextra -Wpedantic)

    # The Xcode generator passes this on its own (it is an Xcode project
    # default), which is how 15 long->int narrowings surfaced there while every
    # Ninja build sat at zero. Pinning it here makes the two generators agree on
    # what zero means. Clang-only: GCC's nearest spelling is -Wconversion, which
    # is the wider, deliberate decision that item reserves.
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND DOOM_WARNINGS_ON -Wshorten-64-to-32)
    endif ()
endif ()

# 1993 C compiled as C++: it warns copiously and none of it is news. The blanket
# is per-file, and a file leaves it the moment it is genuinely rewritten - which
# is why the rewritten sources below are compiled with warnings on instead. New
# code is held to eacp's standard from its first line; vanilla is not held to
# anything until someone rewrites it.
set_source_files_properties(${DOOM_VANILLA_SOURCES} DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES COMPILE_OPTIONS "${DOOM_WARNINGS_OFF}")

set_source_files_properties(${DOOM_REWRITTEN_SOURCES} DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES COMPILE_OPTIONS "${DOOM_WARNINGS_ON}")

# The simulation is not strictly integer-only: FixedDiv2 divides through a double
# (Math/Fixed), and the demos replay because that is the same IEEE-754 operation
# every time. Nothing may fuse it into an FMA, which rounds differently. No
# contraction is possible there today - a divide and a multiply, with no add
# between them - but determinism the goldens rest on should be stated rather than
# left to luck. eacp pins the same flag on its numeric kernels for the same reason.
#
# The same driver split applies, and here it had teeth: clang-cl took the flag as
# `-ffp-contract=off`, did not recognise it, and said so once per translation unit
# ("unknown argument ignored in clang-cl"). The engine was building *without* the
# guarantee on the one Windows toolchain most likely to contract, and the warning
# saying so was lost among the other 44,000. /clang: hands it to the clang front
# end, which is where it was always meant to go. Real MSVC has no equivalent knob
# but defaults to /fp:precise, which does not contract; state it rather than
# inherit it, since /fp:fast in a stray toolchain file would silently desync.
if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(doom-engine PRIVATE /clang:-ffp-contract=off)
    else ()
        target_compile_options(doom-engine PRIVATE /fp:precise)
    endif ()
else ()
    target_compile_options(doom-engine PRIVATE -ffp-contract=off)
endif ()
