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
//   Menu widget stuff, episode selection and such.
//    
//-----------------------------------------------------------------------------

#pragma once


#include "d_event.h"


//
// MENUS
//
 
// Called by main loop,
// saves config file and calls Doom::quitGame when user exits.
// Even when the menu is not displayed,
// this can resize the view and change game parameters.
// Does all the real work of the menu interaction.

// Called by main loop,
// only used for menu (skull cursor) animation.

// Called by main loop,
// draws the menus directly into the screen buffer.

// Called by Doom::doomMain,
// loads the config file.

// Called by intro code to force menu up upon a keypress,
// does nothing if menu is already up.



// Raised while the menu is showing a yes/no prompt rather than a menu proper.
// M_Drawer returns before its background darkening in that case, so anything
// reproducing the darkening has to test this too.
extern int messageToPrint;



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
