#pragma once

#include "../r_defs.h" // seg_t, node_t, drawseg_t

namespace Doom
{
// BSP traversal; r_bsp.cpp keeps the vanilla R_ names as shims.
void clearDrawSegs(void);
void clipSolidWallSegment(int first, int last);
void clipPassWallSegment(int first, int last);
void clearClipSegs(void);
void addLine(seg_t* line);
doom_boolean checkBBox(fixed_t* bspcoord);
void subsector(int num);
void renderBSPNode(int bspnum);
} // namespace Doom
