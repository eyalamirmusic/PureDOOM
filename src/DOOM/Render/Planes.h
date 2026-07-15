#pragma once

#include "../r_defs.h" // visplane_t

namespace Doom
{
// Visplane (floor/ceiling) rendering; r_plane.cpp keeps the vanilla R_ names as shims.
void initPlanes(void);
void mapPlane(int y, int x1, int x2);
void clearPlanes(void);
visplane_t* findPlane(fixed_t height, int picnum, int lightlevel);
visplane_t* checkPlane(visplane_t* pl, int start, int stop);
void makeSpans(int x, int t1, int b1, int t2, int b2);
void drawPlanes(void);
} // namespace Doom
