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
//        Refresh module, BSP traversal and handling.
//
//-----------------------------------------------------------------------------

#pragma once


#include "r_defs.h"


// The BSP traversal pointers and the drawseg pool below are Doom::BSPScratch members
// (Engine); these are references onto them (REFACTOR.md, Step 5).

// rw_x/rw_stopx/segtextured/markfloor/markceiling are Doom::SegState members (Engine); references.


// false if the back side is the same plane

extern doom_boolean skymap;


extern Doom::LightTable** hscalelight;
extern Doom::LightTable** vscalelight;
extern Doom::LightTable** dscalelight;




// BSP?



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
