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
//        Gamma correction LUT.
//        Functions to draw patches (by post) directly to screen.
//        Functions to blit a block to the screen.
//
//-----------------------------------------------------------------------------

#pragma once

#include "doomtype.h"
#include "doomdef.h"
#include "r_data.h" // Needed because we are refering to patches.

#include <ea_data_structures/Structures/Array.h>

//
// VIDEO
//

#define CENTERY (SCREENHEIGHT / 2)


// Screen 0 is the screen updated by I_Update screen.
// Screen 1 is an extra buffer.
extern byte* screens[5];
extern EA::Array<int, 4>& dirtybox; // Doom::VideoState member (Engine); reference
extern EA::Array<EA::Array<byte, 256>, 5> gammatable;
// usegamma is a config-backed Engine member (UI/MenuSettings.h); reference onto it.
extern int& usegamma;


// Allocates buffer screens, call before Doom::renderInit.





// Draw a linear block of pixels into the view buffer.

// Reads a linear block of pixels into the view buffer.



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
