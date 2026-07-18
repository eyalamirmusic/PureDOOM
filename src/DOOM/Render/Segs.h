#pragma once

#include "../r_defs.h" // DrawSeg

namespace Doom
{
// Wall/seg rendering; r_segs.cpp keeps the vanilla R_ names as shims.
void renderMaskedSegRange(DrawSeg* ds, int x1, int x2);
void renderSegLoop();
void storeWallRange(int start, int stop);
} // namespace Doom
