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

#include "d_player.h"
#include "r_data.h"


//
// POV related. viewcos/viewsin live in Doom::ViewPoint now (see r_state.h); these
// are references onto it.
//

// The view window geometry lives in Doom::ViewWindow now (see r_state.h); references.

// The screen projection lives in Doom::ViewProjection now (see r_state.h); these are
// references onto it.




//
// Lighting LUT.
// Used for z-depth cuing per column/row,
// and other lighting effects (sector ambient, flash).
//

// Lighting constants.
// Now why not 32 levels here?
#define LIGHTLEVELS 16
#define LIGHTSEGSHIFT 4

#define MAXLIGHTSCALE 48
#define LIGHTSCALESHIFT 12
#define MAXLIGHTZ 128
#define LIGHTZSHIFT 20

// The light selection lives in Doom::Lighting (an Engine member) now; these are
// references onto it (REFACTOR.md, Step 5), the tables as references-to-array so their
// type and every indexed read are unchanged.



// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.
#define NUMCOLORMAPS 32


// Blocky/low detail mode.
//B remove this?
//  0 = high, 1 = low. Lives in Doom::ViewWindow now (see r_state.h); a reference onto it.


//
// Function pointers to switch refresh/drawing functions.
// Used to select shadow mode etc.
//
extern void (*colfunc)();
extern void (*basecolfunc)();
extern void (*fuzzcolfunc)();
// No shadow effects on floors.
extern void (*spanfunc)();


//
// Utility functions.

// Places the camera and picks the light tables for a frame, from the player's
// position and their fixedcolormap (the invulnerability sphere and the light-amp
// visor lock the view to one COLORMAP row, overriding light and distance both).

// How fast light falls off with distance in the scale-light table. Anything
// reproducing DOOM's shading has to use the same number or the banding differs.
#define DISTMAP 2


//
// REFRESH - the actual rendering functions.
//

// Called by G_Drawer.

// Called by startup code.

// Called by Doom::menuResponder.


//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
