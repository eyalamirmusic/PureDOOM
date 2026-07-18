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
//
// DESCRIPTION:
//        System interface, sound.
//
//-----------------------------------------------------------------------------

#pragma once


#include "doomdef.h"



#include "doomstat.h"
#include "sounds.h"


// Init at program start...

// ... update sound buffer and audio device at runtime...

// ... shut down and relase at program termination.


//
//  SFX I/O
//

// Initialize channels?

// Get raw data lump index for sound descriptor.

// Starts a sound in a particular sound channel.

// Stops a sound channel.

// Called by S_*() functions
//  to see if a channel is still playing.
// Returns 0 if no longer playing, 1 if playing.

// Updates the volume, separation,
//  and pitch of a sound channel.


//
//  MUSIC I/O
//

// Volume.

// PAUSE game handling.

// Registers a song handle to song data.

// Called by anything that wishes to start music.
//  plays a song, and when the song is done,
//  starts playing it again in an endless loop.
// Horrible thing to do, considering.

// Stops a song over 3 seconds.

// See above (register), then think backwards

// Get next MIDI message



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
