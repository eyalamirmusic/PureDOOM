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
//
//    
//-----------------------------------------------------------------------------

#pragma once


#include "doomtype.h"


// Returns a number from 0 to 255,
// from a lookup table.
int M_Random(void);

// As M_Random, but used only by the play simulation.
int P_Random(void);

// Fix randoms for demos.
void M_ClearRandom(void);


// DOOM is not random at all: it walks a fixed 256-entry table with an index, and
// keeps two indices into it.
//
// prndindex is P_Random's, and it is the one the game world turns on - damage
// rolls, monster decisions, weapon spread. A demo replays byte-identically only
// because this sequence is reproduced exactly, so any change that adds, drops or
// reorders a single P_Random call shifts everything after it and the world
// diverges. rndindex is M_Random's, for everything outside the simulation, and
// is deliberately kept apart so that sounds and menus can vary without desyncing
// the game.
//
// Exported so the tests can watch them; they are the sharpest canary there is
// for an accidental change to the simulation.
//
// References, not variables: Doom::Random (Sim/Random.h) owns the state now, and
// these are the vanilla names for the same four bytes. They read and write the
// object, so a caller that has not been rewritten yet and one that has cannot
// disagree about where the world's chance comes from.
extern int& rndindex;
extern int& prndindex;
extern const unsigned char* rndtable;



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
