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
#include "Render/PlaneScratch.h"
#include "Render/RenderScratch.h"

#define MAXVISPLANES        128
#define MAXOPENINGS        SCREENWIDTH*64

// floorfunc (and the never-defined ceilingfunc_t extern in r_plane.h) were vestigial - defined
// or declared but never called - and are deleted, the way the parallel ceilingfunc was.

//
// opening
//

// Here comes the obnoxious "visplane". The current subsector's floor/ceiling visplanes
// live in Doom::RenderScratch (an Engine member) now; these are references onto it.
visplane_t*& floorplane = Doom::renderScratch().floorplane;
visplane_t*& ceilingplane = Doom::renderScratch().ceilingplane;

// The cross-read plane state (lastopening, the clip arrays and the projection tables) is a
// Doom::PlaneScratch owned by the Engine now (alongside the openings lastopening points into);
// these are references onto its members (REFACTOR.md, Step 5).
short*& lastopening = Doom::planeScratch().lastopening;

//
// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1
//
short (&floorclip)[SCREENWIDTH] = Doom::planeScratch().floorclip;
short (&ceilingclip)[SCREENWIDTH] = Doom::planeScratch().ceilingclip;

//
// spanstart holds the start of a plane span
// initialized to 0 at start
//

//
// texture mapping
//

fixed_t (&yslope)[SCREENHEIGHT] = Doom::planeScratch().yslope;
fixed_t (&distscale)[SCREENWIDTH] = Doom::planeScratch().distscale;



//
// R_InitPlanes
// Only at game startup.
//







