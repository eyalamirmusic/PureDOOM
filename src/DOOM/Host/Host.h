#pragma once

#include "../DOOM.h" // the doom_*_fn callback typedefs

namespace Doom
{
// The platform hooks the embedder hands the engine: printing, allocation, file I/O,
// the clock, exit and getenv. doom_set_* (DOOM.h) write these before doom_init, which
// then defaults any left null to the built-in *_impl (Host/Api.cpp).
//
// Unlike the Engine's state - which *is* the world, and which a test wants a fresh copy
// of - these are host/platform state: set once by the embedder and the same whichever
// world is running. So when engine() eventually becomes a constructed instance rather
// than a singleton, the Host must NOT be reset with it; it therefore lives in its own
// singleton, host(), parallel to engine() but deliberately separate (REFACTOR.md, the
// doom_config->Host fold). This is a loose-global cleanup: the 13 callbacks were
// file-scope globals in Api.cpp, gathered here into one owner. The vanilla names
// doom_print / doom_malloc / ... become references onto these members (doom_config.h),
// so the ~380 call sites and the doom_set_* API are unchanged.
struct Host
{
    doom_print_fn print = nullptr;
    doom_malloc_fn malloc = nullptr;
    doom_free_fn free = nullptr;
    doom_open_fn open = nullptr;
    doom_close_fn close = nullptr;
    doom_read_fn read = nullptr;
    doom_write_fn write = nullptr;
    doom_seek_fn seek = nullptr;
    doom_tell_fn tell = nullptr;
    doom_eof_fn eof = nullptr;
    doom_gettime_fn gettime = nullptr;
    doom_exit_fn exit = nullptr;
    doom_getenv_fn getenv = nullptr;
};

// The one Host. A function-local static (Host.cpp), so it is constructed on the first
// access - which is Api.cpp binding the doom_* references at static-init time, before
// main() and before any doom_set_*.
Host& host();
} // namespace Doom
