#pragma once

#include "../Game/GameDefs.h" // SCREENHEIGHT
#include "../Math/Vec.h"
#include "../doomtype.h" // byte
#include "../Sim/MapTypes.h"
#include "RenderTypes.h" // Patch

#include "../Containers.h"

#include "VideoState.h" // screens[] is a VideoState member now; gammatable was dead

namespace Doom
{
// Low-level framebuffer drawing; v_video.cpp keeps the vanilla V_ names as shims.
void markRect(Vec2i at, Vec2i size);
void copyRect(Vec2i srcAt, int srcscrn, Vec2i size, Vec2i destAt, int destscrn);
void drawPatch(Vec2i at, int scrn, Patch* patch);
void drawPatchFlipped(Vec2i at, int scrn, Patch* patch);
void drawPatchRectDirect(Vec2i at, int scrn, Patch* patch, int src_x, int src_w);
void drawPatchDirect(Vec2i at, int scrn, Patch* patch);
void drawBlock(Vec2i at, int scrn, Vec2i size, byte* src);
} // namespace Doom
