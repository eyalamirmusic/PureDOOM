#pragma once

#include "../r_defs.h" // drawseg_t

namespace Doom
{
// Wall/seg rendering; r_segs.cpp keeps the vanilla R_ names as shims.
void renderMaskedSegRange(drawseg_t* ds, int x1, int x2);
void renderSegLoop(void);
void storeWallRange(int start, int stop);
} // namespace Doom
