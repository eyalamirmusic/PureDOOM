# The DOOM engine itself.
#
# This fork owns these sources and modifies them. The app and the test suite
# both link this one library, so a change to the simulation reaches both and
# cannot go stale in one of them. It is the only way to build the engine: the
# generated single header is gone, and with it the rule that no two files may
# share a file-scope name.

file(GLOB DOOM_ENGINE_SOURCES CONFIGURE_DEPENDS *.c *.h)

add_library(doom-engine STATIC ${DOOM_ENGINE_SOURCES})
add_library(DOOM::engine ALIAS doom-engine)

# The engine reaches its host through function pointers, and DOOM.c carries a
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

# 1993 C: it warns copiously and none of it is news. Kept quiet until the
# refactor reaches each file.
target_compile_options(doom-engine PRIVATE $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>)
