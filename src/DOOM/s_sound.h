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
//        The not so system specific sound interface.
//
//-----------------------------------------------------------------------------

#pragma once


//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
// allocates channel buffer, sets S_sfx lookup.
//

//
// Per level startup code.
// Kills playing sounds at start of level,
//  determines music if any, changes music.
//

//
// Start sound for thing at <origin>
//  using <sound_id> from sounds.h
//

// Will start a sound at a given volume.

// Stop sound for thing at <origin>

// Start music using <music_id> from sounds.h

// Start music using <music_id> from sounds.h,
// and set whether looping

// Stops the music fer sure.

// Stop and resume music, during game PAUSE.

//
// Updates music & sounds
//




//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
