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
//        Mission start screen wipe/melt, special effects.
//        
//-----------------------------------------------------------------------------

#ifndef __F_WIPE_H__
#define __F_WIPE_H__


//
// SCREEN WIPE PACKAGE
//

enum
{
    // simple gradual pixel change for 8-bit only
    wipe_ColorXForm,

    // weird screen melt
    wipe_Melt,

    wipe_NUMWIPES
};


int wipe_StartScreen(int x, int y, int width, int height);
int wipe_EndScreen(int x, int y, int width, int height);
int wipe_ScreenWipe(int wipeno, int x, int y, int width, int height, int ticks);


//
// The melt's state, for a renderer that composites the wipe itself rather than
// letting wipe_doMelt blit into the frame buffer.
//
// The melt only ever READS the outgoing screen and slides it down over whatever
// is beneath, so a renderer needs the outgoing frame and how far each column has
// moved - and nothing else. The incoming frame is just what is already there.
//

// Raised while a melt is running. wipe_exitMelt frees the column table without
// clearing the pointer to it, so this is the only safe thing to test.
extern doom_boolean wipe_melt_running;

// The outgoing frame, as palette indices. wipe_initMelt leaves it column-major.
extern byte* wipe_scr_start;

// How far down each two-pixel column has slid, in rows. Negative means the
// column has not started moving yet.
extern int* wipe_melt_offsets;


#endif

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
