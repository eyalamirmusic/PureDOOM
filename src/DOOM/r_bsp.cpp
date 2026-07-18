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
#include "Render/BSPScratch.h"

#include "Render/Segs.h"
#define MAXSEGS 32

// The BSP traversal pointers and the drawseg pool are a Doom::BSPScratch owned by the Engine
// now; these are references onto its members (REFACTOR.md, Step 5).
seg_t*& curline = Doom::bspScratch().curline;

side_t*& sidedef = Doom::bspScratch().sidedef;

line_t*& linedef = Doom::bspScratch().linedef;

sector_t*& frontsector = Doom::bspScratch().frontsector;

sector_t*& backsector = Doom::bspScratch().backsector;


drawseg_t (&drawsegs)[MAXDRAWSEGS] = Doom::bspScratch().drawsegs;

drawseg_t*& ds_p = Doom::bspScratch().ds_p;



void Doom::storeWallRange(int start, int stop);



//
// R_ClearDrawSegs
//








