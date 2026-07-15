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
file(GLOB DOOM_REWRITTEN_SOURCES CONFIGURE_DEPENDS
        Math/*.cpp Math/*.h
        Sim/*.cpp Sim/*.h
        Render/*.cpp Render/*.h
        Wad/*.cpp Wad/*.h
        Engine/*.cpp Engine/*.h)

add_library(doom-engine STATIC ${DOOM_VANILLA_SOURCES} ${DOOM_REWRITTEN_SOURCES})
add_library(DOOM::engine ALIAS doom-engine)

target_compile_features(doom-engine PUBLIC cxx_std_20)

# The engine reaches its host through function pointers, and DOOM.cpp carries a
# default implementation of each, gated on these. A host that wants its own
# (audio, say) sets the pointer after doom_init instead.
target_compile_definitions(doom-engine
        PRIVATE
        DOOM_IMPLEMENT_PRINT
        DOOM_IMPLEMENT_MALLOC
        DOOM_IMPLEMENT_FILE_IO
        DOOM_IMPLEMENT_SOCKETS
        DOOM_IMPLEMENT_GETTIME
        DOOM_IMPLEMENT_EXIT
        DOOM_IMPLEMENT_GETENV)

# Consumers include <DOOM/DOOM.h>. The engine's own files include each other by
# bare name, which resolves next to them.
target_include_directories(doom-engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# 1993 C compiled as C++: it warns copiously and none of it is news. The blanket
# is per-file, and a file leaves it the moment it is genuinely rewritten - which
# is why the rewritten sources below are compiled with warnings on instead. New
# code is held to eacp's standard from its first line; vanilla is not held to
# anything until someone rewrites it.
set_source_files_properties(${DOOM_VANILLA_SOURCES} DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>")

set_source_files_properties(${DOOM_REWRITTEN_SOURCES} DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES COMPILE_OPTIONS
        "$<IF:$<CXX_COMPILER_ID:MSVC>,/W4,-Wall;-Wextra;-Wpedantic>")

# The simulation is not strictly integer-only: FixedDiv2 divides through a double
# (m_fixed.cpp), and the demos replay because that is the same IEEE-754 operation
# every time. Nothing may fuse it into an FMA, which rounds differently. No
# contraction is possible there today - a divide and a multiply, with no add
# between them - but determinism the goldens rest on should be stated rather than
# left to luck. eacp pins the same flag on its numeric kernels for the same reason.
target_compile_options(doom-engine PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-ffp-contract=off>)
