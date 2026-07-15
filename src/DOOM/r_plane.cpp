// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Floor/ceiling (visplane) rendering. Rewritten in Render/Planes.{h,cpp};
//        this keeps the R_ names as shims and owns the visplane/span globals.
//
//-----------------------------------------------------------------------------

#include "r_local.h"

#include "Render/Planes.h"
#include "Render/RenderScratch.h"

#define MAXVISPLANES        128
#define MAXOPENINGS        SCREENWIDTH*64

planefunction_t floorfunc;

//
// opening
//

// Here comes the obnoxious "visplane". The current subsector's floor/ceiling visplanes
// live in Doom::RenderScratch (an Engine member) now; these are references onto it.
visplane_t*& floorplane = Doom::renderScratch().floorplane;
visplane_t*& ceilingplane = Doom::renderScratch().ceilingplane;

// ?
short* lastopening;

//
// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1
//
short floorclip[SCREENWIDTH];
short ceilingclip[SCREENWIDTH];

//
// spanstart holds the start of a plane span
// initialized to 0 at start
//

//
// texture mapping
//

fixed_t yslope[SCREENHEIGHT];
fixed_t distscale[SCREENWIDTH];



//
// R_InitPlanes
// Only at game startup.
//

void R_InitPlanes(void)
{
    Doom::initPlanes();
}

void R_MapPlane(int y, int x1, int x2)
{
    Doom::mapPlane(y, x1, x2);
}

void R_ClearPlanes(void)
{
    Doom::clearPlanes();
}

visplane_t* R_FindPlane(fixed_t height, int picnum, int lightlevel)
{
    return Doom::findPlane(height, picnum, lightlevel);
}

visplane_t* R_CheckPlane(visplane_t* pl, int start, int stop)
{
    return Doom::checkPlane(pl, start, stop);
}

void R_MakeSpans(int x, int t1, int b1, int t2, int b2)
{
    Doom::makeSpans(x, t1, b1, t2, b2);
}

void R_DrawPlanes(void)
{
    Doom::drawPlanes();
}
