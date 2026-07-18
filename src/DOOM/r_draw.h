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

// first pixel in a column


// The span blitting interface.
// Hook in assembler or system specific BLT
//  here.

// The Spectre/Invisibility effect.
void R_DrawFuzzColumnLow();

// Draw with color translation tables,
//  for player sprite rendering,
//  Green/Red/Blue/Indigo shirts.
void R_DrawTranslatedColumnLow();





// start of a 64*64 tile image

// A 256-byte-aligned view onto DrawState's owned translationTableStorage (Step 9).
extern byte* translationtables;


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
