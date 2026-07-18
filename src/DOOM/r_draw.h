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
//        System specific interface stuff.
//
//-----------------------------------------------------------------------------

#pragma once


#include "r_defs.h"


// The drawer inputs are Doom::DrawState members (Engine) now; references onto them (Step 5).
extern lighttable_t*& dc_colormap;
extern int& dc_x;
extern int& dc_yl;
extern int& dc_yh;
extern fixed_t& dc_iscale;
extern fixed_t& dc_texturemid;

// first pixel in a column
extern byte*& dc_source;


// The span blitting interface.
// Hook in assembler or system specific BLT
//  here.

// The Spectre/Invisibility effect.
void R_DrawFuzzColumnLow();

// Draw with color translation tables,
//  for player sprite rendering,
//  Green/Red/Blue/Indigo shirts.
void R_DrawTranslatedColumnLow();


extern int& ds_y;
extern int& ds_x1;
extern int& ds_x2;

extern lighttable_t*& ds_colormap;

extern fixed_t& ds_xfrac;
extern fixed_t& ds_yfrac;
extern fixed_t& ds_xstep;
extern fixed_t& ds_ystep;

// start of a 64*64 tile image
extern byte*& ds_source;

// A 256-byte-aligned view onto DrawState's owned translationTableStorage (Step 9).
extern byte* translationtables;
extern byte*& dc_translation;


// Span blitting for rows, floor/ceiling.
// No Sepctre effect needed.

// Low resolution mode, 160x200?



// Initialize color translation tables,
//  for player rendering etc.

// Rendering function.

// If the view size is not full screen, draws a border around it.



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
