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
Doom::Seg*& curline = Doom::bspScratch().curline;

Doom::Side*& sidedef = Doom::bspScratch().sidedef;

Doom::Line*& linedef = Doom::bspScratch().linedef;

Doom::Sector*& frontsector = Doom::bspScratch().frontsector;

Doom::Sector*& backsector = Doom::bspScratch().backsector;


Doom::DrawSeg (&drawsegs)[MAXDRAWSEGS] = Doom::bspScratch().drawsegs;

Doom::DrawSeg*& ds_p = Doom::bspScratch().ds_p;



void Doom::storeWallRange(int start, int stop);



//
// R_ClearDrawSegs
//








