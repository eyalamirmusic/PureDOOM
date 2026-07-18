#pragma once

#include "../i_system.h" // vanilla I_ system interface + ticcmd_t / byte

namespace Doom
{
// The engine's system seam: timing (currentTic), the zone's backing allocation
// (zoneBase / heapSize), startup and teardown (initHost / quitGame) and the
// fatal path (fatalError, which the tests catch by way of doom_set_exit). Most of
// the rest are host stubs. i_system.cpp keeps the vanilla I_ names as shims over
// these; mb_used and emptycmd are file-local to System.cpp.
void tactileFeedback(int on, int off, int total);
ticcmd_t* baseTiccmd();
int heapSize();
byte* zoneBase(int* size);
int currentTic();
void initHost();
void quitGame();
void waitVBlank(int count);
void beginRead();
void endRead();
byte* allocLow(int length);
void fatalError(const char* error);
} // namespace Doom
