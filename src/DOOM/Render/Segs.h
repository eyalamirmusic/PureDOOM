#pragma once

#include "../Sim/MapTypes.h"
#include "RenderTypes.h" // DrawSeg

namespace Doom
{
// Wall/seg rendering; r_segs.cpp keeps the vanilla R_ names as shims. The masked
// (two-sided middle) pass is DrawSeg::renderMaskedRange, keyed off one drawseg and
// declared in RenderTypes.h; its body is in Segs.cpp with these.
void renderSegLoop();
void storeWallRange(int start, int stop);
} // namespace Doom
