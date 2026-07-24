#pragma once

// _WIN32 is the macro every Windows compiler predefines, on 32- and 64-bit and
// on ARM64 alike. Bare WIN32 is not a compiler macro at all - it arrives from
// the Windows SDK, or from a build system that adds -DWIN32 by hand, which CMake
// does happen to do for MSVC-style drivers. Testing for it therefore worked here
// only by way of CMake's default flags: compile this engine any other way and
// every Windows build silently took the DOOM_LINUX branch.
//
// DOOM_LINUX is defined and never tested - the two branches that carry code are
// DOOM_WIN32 and DOOM_APPLE. It stays anyway, unlike the dead constants the macro
// sweep deleted: it is one of three names for the same question, and dropping the
// fallback would leave an `#else` that identifies no platform at all.
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

// The doom_print / doom_malloc / ... aliases that used to sit here are gone. They
// were thirteen extern references bound at static-init time onto Doom::host()'s
// members, plus doom_flags beside them; call sites reach host() directly now, which
// is the singleton they were always naming.

void doom_memset(void* ptr, int value, int num);
void* doom_memcpy(void* destination, const void* source, int num);
