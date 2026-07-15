// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        BSP traversal. Rewritten in Render/BSP.{h,cpp}; this keeps the R_ names as
//        shims and owns the drawseg/clip globals.
//
//-----------------------------------------------------------------------------

#include "r_local.h"

#include "Render/BSP.h"

#define MAXSEGS 32

seg_t* curline;

side_t* sidedef;

line_t* linedef;

sector_t* frontsector;

sector_t* backsector;


drawseg_t drawsegs[MAXDRAWSEGS];

drawseg_t* ds_p;



void R_StoreWallRange(int start, int stop);



//
// R_ClearDrawSegs
//

void R_ClearDrawSegs(void)
{
    Doom::clearDrawSegs();
}

void R_ClipSolidWallSegment(int first, int last)
{
    Doom::clipSolidWallSegment(first, last);
}

void R_ClipPassWallSegment(int first, int last)
{
    Doom::clipPassWallSegment(first, last);
}

void R_ClearClipSegs(void)
{
    Doom::clearClipSegs();
}

void R_AddLine(seg_t* line)
{
    Doom::addLine(line);
}

doom_boolean R_CheckBBox(fixed_t* bspcoord)
{
    return Doom::checkBBox(bspcoord);
}

void R_Subsector(int num)
{
    Doom::subsector(num);
}

void R_RenderBSPNode(int bspnum)
{
    Doom::renderBSPNode(bspnum);
}
