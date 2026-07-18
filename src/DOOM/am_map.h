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
//  AutoMap module.
//
//-----------------------------------------------------------------------------

#pragma once


#include "d_event.h"
#include "d_player.h"
#include "doomdata.h"
#include "m_fixed.h"
#include "tables.h"


// Used by ST StatusBar stuff.
#define AM_MSGHEADER (('a'<<24)+('m'<<16))
#define AM_MSGENTERED (AM_MSGHEADER | ('e'<<8))
#define AM_MSGEXITED (AM_MSGHEADER | ('x'<<8))


// Called by main loop.
doom_boolean AM_Responder(event_t* ev);

// Called by main loop.
void AM_Ticker();

// Called by main loop,
// called instead of view drawer if automap active.
void AM_Drawer();

// Called to force the automap to quit
// if the level is completed while it is up.
void AM_Stop();


//
// What the automap is made of, for a renderer that wants to draw it as
// something other than lines poked into a 320x200 frame buffer.
//
// AM_Drawer rasterises the map with Bresenham straight into the frame. A GPU
// renderer wants the same decisions - the same lines, the same colours, the same
// player arrow, rotated the same way - without the rasterising, so what it needs
// is what AM_Drawer reads, not what it writes. That is all this is: the state and
// the shapes, named. The choices stay AM_Drawer's own.
//

struct mpoint_t
{
    fixed_t x, y;
};


struct mline_t
{
    mpoint_t a, b;
};


// The automap's palette. It picks raw colour indices rather than texturing
// anything, and these are the ones it picks.
#define REDS        (256-5*16)
#define REDRANGE    16
#define BLUES       (256-4*16+8)
#define BLUERANGE   8
#define GREENS      (7*16)
#define GREENRANGE  16
#define GRAYS       (6*16)
#define GRAYSRANGE  16
#define BROWNS      (4*16)
#define BROWNRANGE  16
#define YELLOWS     (256-32+7)
#define YELLOWRANGE 1
#define BLACK       0
#define WHITE       (256-47)

#define BACKGROUND          BLACK
#define YOURCOLORS          WHITE
#define WALLCOLORS          REDS
#define WALLRANGE           REDRANGE
#define TSWALLCOLORS        GRAYS
#define FDWALLCOLORS        BROWNS
#define CDWALLCOLORS        YELLOWS
#define THINGCOLORS         GREENS
#define SECRETWALLCOLORS    WALLCOLORS
#define GRIDCOLORS          (GRAYS + GRAYSRANGE/2)
#define XHAIRCOLORS         GRAYS

// A line the map never draws, whatever the player has seen.
#define LINE_NEVERSEE ML_DONTDRAW


// The line drawings the map is made of. Their sizes are asserted against the
// definitions in am_map.c: a count that drifts is a compile error, not a
// renderer walking off the end of an array.
#define NUMPLYRLINES 7
#define NUMCHEATPLYRLINES 16
#define NUMTHINTRIANGLEGUYLINES 3

extern mline_t player_arrow[NUMPLYRLINES];
extern mline_t cheat_player_arrow[NUMCHEATPLYRLINES];
extern mline_t thintriangle_guy[NUMTHINTRIANGLEGUYLINES];


// Where the map is looking, and how far in. m_x/m_y is the lower-left corner in
// map coordinates; scale_mtof converts a map distance to a frame one.
extern fixed_t m_x, m_y;

// How much of the map the window spans, in map coordinates.
extern fixed_t m_w, m_h;

extern fixed_t scale_mtof;

// The map's rect within the frame.
extern int f_x, f_y, f_w, f_h;

// The player it draws the arrow for, and whether it is keeping them centred.
extern player_t* am_plr;
extern int followplayer;

// The map cheats: `cheating` reveals the walls and the things, `grid` the grid.
extern int cheating;
extern int grid;

// Wall brightness, which the map strobes.
extern int lightlev;


// Turns a map point about the origin. The automap's own, used as it stands, so a
// renderer puts the player arrow through exactly the rotation AM_Drawer would.
void AM_rotate(fixed_t* x, fixed_t* y, angle_t a);

// Draws the player's marks into the frame.
void AM_drawMarks();



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
