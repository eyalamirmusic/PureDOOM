#pragma once

#include "../i_video.h" // the vanilla I_ video interface + byte

namespace Doom
{
// The engine's video seam. In PureDOOM this is a thin host stub - the real
// drawing is the eacp app's, reading screens[0] and the palette back out - so
// most of these are empty. i_video.cpp keeps the vanilla I_ names as shims over
// these; screen_palette stays at file scope in Video.cpp for its many readers
// (DOOM.cpp, the menu, the status bar, the eacp port, the frame hash).
void I_InitGraphics();
void I_ShutdownGraphics();
void I_SetPalette(byte* palette);
void I_UpdateNoBlit();
void I_FinishUpdate();
void I_ReadScreen(byte* scr);
void I_StartFrame();
void I_StartTic();
void I_GetEvent();
} // namespace Doom
