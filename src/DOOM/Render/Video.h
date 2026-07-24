#pragma once

#include "../Game/GameDefs.h" // SCREENHEIGHT
#include "../doomtype.h" // byte
#include "../Sim/MapTypes.h"
#include "RenderTypes.h" // Patch

#include "../Containers.h"

#include "VideoState.h" // screens[] is a VideoState member now; gammatable was dead

namespace Doom
{
// Low-level framebuffer drawing; v_video.cpp keeps the vanilla V_ names as shims.
void markRect(int x, int y, int width, int height);
void copyRect(int srcx,
              int srcy,
              int srcscrn,
              int width,
              int height,
              int destx,
              int desty,
              int destscrn);
void drawPatch(int x, int y, int scrn, Patch* patch);
void drawPatchFlipped(int x, int y, int scrn, Patch* patch);
void drawPatchRectDirect(int x, int y, int scrn, Patch* patch, int src_x, int src_w);
void drawPatchDirect(int x, int y, int scrn, Patch* patch);
void drawBlock(int x, int y, int scrn, int width, int height, byte* src);
void getBlock(int x, int y, int scrn, int width, int height, byte* dest);
void initVideo();
} // namespace Doom
