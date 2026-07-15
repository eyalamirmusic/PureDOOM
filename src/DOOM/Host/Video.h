#pragma once

#include "../i_video.h" // the vanilla I_ video interface + byte

namespace Doom
{
// The engine's video seam. In PureDOOM this is a thin host stub - the real
// drawing is the eacp app's, reading screens[0] and the palette back out - so
// most of these are empty. i_video.cpp keeps the vanilla I_ names as shims over
// these; screen_palette stays at file scope in Video.cpp for its many readers
// (DOOM.cpp, the menu, the status bar, the eacp port, the frame hash).
void I_InitGraphics(void);
void I_ShutdownGraphics(void);
void I_SetPalette(byte* palette);
void I_UpdateNoBlit(void);
void I_FinishUpdate(void);
void I_ReadScreen(byte* scr);
void I_StartFrame(void);
void I_StartTic(void);
void I_GetEvent(void);
} // namespace Doom
