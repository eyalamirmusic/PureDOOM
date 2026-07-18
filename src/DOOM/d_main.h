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
// $Log:$
//
// DESCRIPTION:
//        System specific interface stuff.
//
//-----------------------------------------------------------------------------

#pragma once


#include "d_event.h"


#define MAXWADFILES 20


// wadfiles[] is file-local to Game/DoomMain.cpp (a static there) - the boot-time WAD
// list D_AddFile builds and Doom::initWadFiles consumes, read by no other file.



//
// D_DoomMain()
// Not a globally visible function, just included for source reference,
// calls all startup code, parses command line options.
// If not overrided by user input, calls N_AdvanceDemo.
//

// Called by IO functions when input is detected.

//
// BASE LEVEL
//

// The attract loop's "move on to the next screen" flag. D_DoAdvanceDemo clears
// gameaction and picks its own demo, so a host that wants to drive one itself
// has to lower this first, or the title sequence takes the game straight back.
// advancedemo is a member of the Doom::AttractMode owned by the Engine now; this is a
// reference onto it (REFACTOR.md, Step 5).
extern doom_boolean& advancedemo;


