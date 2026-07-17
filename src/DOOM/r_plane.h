// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//        Refresh, visplane stuff (floor, ceilings).
//
//-----------------------------------------------------------------------------

#pragma once


#include "r_data.h"


// Visplane related. These are Doom::PlaneScratch members (Engine) now; references onto them
// (REFACTOR.md, Step 5). floorfunc and the never-defined ceilingfunc_t were vestigial and were
// deleted (the parallel ceilingfunc went the same way).
extern short*& lastopening;

typedef void (*planefunction_t) (int top, int bottom);

extern short (&floorclip)[SCREENWIDTH];
extern short (&ceilingclip)[SCREENWIDTH];

extern fixed_t (&yslope)[SCREENHEIGHT];
extern fixed_t (&distscale)[SCREENWIDTH];

void R_InitPlanes(void);
void R_ClearPlanes(void);
void R_MapPlane(int y, int x1, int x2);
void R_MakeSpans(int x, int t1, int b1, int t2, int b2);
void R_DrawPlanes(void);
visplane_t* R_FindPlane(fixed_t height, int picnum, int lightlevel);
visplane_t* R_CheckPlane(visplane_t* pl, int start, int stop);



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
