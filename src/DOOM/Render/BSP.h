#pragma once

#include "../Sim/MapTypes.h"
#include "RenderTypes.h" // Seg, Node, DrawSeg

namespace Doom
{
// BSP traversal; r_bsp.cpp keeps the vanilla R_ names as shims.
void clearDrawSegs();
void clipSolidWallSegment(int first, int last);
void clipPassWallSegment(int first, int last);
void clearClipSegs();
void addLine(Seg* line);
bool checkBBox(fixed_t* bspcoord);
void subsector(int num);
void renderBSPNode(int bspnum);
} // namespace Doom
