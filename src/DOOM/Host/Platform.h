#pragma once

// _WIN32 is the macro every Windows compiler predefines, on 32- and 64-bit and
// on ARM64 alike. Bare WIN32 is not a compiler macro at all - it arrives from
// the Windows SDK, or from a build system that adds -DWIN32 by hand, which CMake
// does happen to do for MSVC-style drivers. Testing for it therefore worked here
// only by way of CMake's default flags: compile this engine any other way and
// every Windows build silently took the DOOM_LINUX branch.
#if defined(_WIN32)
#define DOOM_WIN32
#elif defined(__APPLE__)
#define DOOM_APPLE
#else
#define DOOM_LINUX
#endif

#include "../DOOM.h"

// A template rather than a macro so it works on Doom::Fixed as well as on ints
// (and evaluates its argument once, which the macro did not).
template <typename T>
constexpr T doom_abs(T x)
{
    return x < T {} ? -x : x;
}

extern int doom_flags;

// The 13 host callbacks are references onto Doom::host()'s members (Host/Host.h) - the
// storage moved off these loose globals into one Host owner (REFACTOR.md, the
// doom_config->Host fold). Every reader resolves through the reference unchanged.
extern Doom::PrintHandler& doom_print;
extern Doom::MallocHandler& doom_malloc;
extern Doom::FreeHandler& doom_free;
extern Doom::OpenHandler& doom_open;
extern Doom::CloseHandler& doom_close;
extern Doom::ReadHandler& doom_read;
extern Doom::WriteHandler& doom_write;
extern Doom::SeekHandler& doom_seek;
extern Doom::TellHandler& doom_tell;
extern Doom::EofHandler& doom_eof;
extern Doom::GetTimeHandler& doom_gettime;
extern Doom::ExitHandler& doom_exit;
extern Doom::GetEnvHandler& doom_getenv;

void doom_memset(void* ptr, int value, int num);
void* doom_memcpy(void* destination, const void* source, int num);
