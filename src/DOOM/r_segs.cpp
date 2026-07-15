// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Wall/seg rendering. Rewritten in Render/Segs.{h,cpp}; this keeps the R_
//        names as shims and owns the wall-column globals.
//
//-----------------------------------------------------------------------------

#include "r_local.h"

#include "Render/RenderScratch.h"
#include "Render/Segs.h"

#define HEIGHTBITS 12
#define HEIGHTUNIT (1<<HEIGHTBITS)

doom_boolean segtextured;


// False if the back side is the same plane.
doom_boolean markfloor;

doom_boolean markceiling;

int toptexture;

int bottomtexture;

int midtexture;


// The current wall segment's projection lives in Doom::RenderScratch (an Engine member)
// now; these are references onto it.
angle_t& rw_normalangle = Doom::renderScratch().rw_normalangle;

// angle to line origin
int& rw_angle1 = Doom::renderScratch().rw_angle1;


//
// regular wall
//
int rw_x;

int rw_stopx;

fixed_t& rw_distance = Doom::renderScratch().rw_distance;


lighttable_t** walllights;


short* maskedtexturecol;



//
// R_RenderMaskedSegRange
//

void R_RenderMaskedSegRange(drawseg_t* ds, int x1, int x2)
{
    Doom::renderMaskedSegRange(ds, x1, x2);
}

void R_RenderSegLoop(void)
{
    Doom::renderSegLoop();
}

void R_StoreWallRange(int start, int stop)
{
    Doom::storeWallRange(start, stop);
}
