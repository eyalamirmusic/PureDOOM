#pragma once

#include "../i_system.h" // vanilla I_ system interface + ticcmd_t / byte

namespace Doom
{
// The engine's system seam: timing (I_GetTime), the zone's backing allocation
// (I_ZoneBase / I_GetHeapSize), startup and teardown (I_Init / I_Quit) and the
// fatal path (I_Error, which the tests catch by way of doom_set_exit). Most of
// the rest are host stubs. i_system.cpp keeps the vanilla I_ names as shims over
// these; mb_used and emptycmd are file-local to System.cpp.
void I_Tactile(int on, int off, int total);
ticcmd_t* I_BaseTiccmd();
int I_GetHeapSize();
byte* I_ZoneBase(int* size);
int I_GetTime();
void I_Init();
void I_Quit();
void I_WaitVBL(int count);
void I_BeginRead();
void I_EndRead();
byte* I_AllocLow(int length);
void I_Error(const char* error);
} // namespace Doom
