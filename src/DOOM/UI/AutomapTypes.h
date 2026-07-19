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

#include "../Game/Event.h"
#include "../Game/PlayerTypes.h"
#include "../Wad/MapFormat.h"
#include "../Math/FixedPoint.h"
#include "../Math/TrigTables.h"

// Used by ST StatusBar stuff.
//
// Everything this header declares stays at global scope, macro or not: it is the
// deliberate carve-out examples/EACP/EngineAccess.cpp reads by bare name, and it
// is not built as part of namespace Doom.
constexpr int AM_MSGHEADER = ('a' << 24) + ('m' << 16);
constexpr int AM_MSGENTERED = AM_MSGHEADER | ('e' << 8);
constexpr int AM_MSGEXITED = AM_MSGHEADER | ('x' << 8);

// Called by main loop.

// Called by main loop.

// Called by main loop,
// called instead of view drawer if automap active.

// Called to force the automap to quit
// if the level is completed while it is up.

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

namespace Doom
{
struct MapPoint
{
    fixed_t x, y;
};
} // namespace Doom

namespace Doom
{
struct MapLine
{
    MapPoint a, b;
};
} // namespace Doom

// The automap's palette. It picks raw colour indices rather than texturing
// anything, and these are the ones it picks.
constexpr int REDS = 256 - 5 * 16;
constexpr int REDRANGE = 16;
constexpr int BLUES = 256 - 4 * 16 + 8;
constexpr int BLUERANGE = 8;
constexpr int GREENS = 7 * 16;
constexpr int GREENRANGE = 16;
constexpr int GRAYS = 6 * 16;
constexpr int GRAYSRANGE = 16;
constexpr int BROWNS = 4 * 16;
constexpr int BROWNRANGE = 16;
constexpr int YELLOWS = 256 - 32 + 7;
constexpr int YELLOWRANGE = 1;
constexpr int BLACK = 0;
constexpr int WHITE = 256 - 47;

constexpr int BACKGROUND = BLACK;
constexpr int YOURCOLORS = WHITE;
constexpr int WALLCOLORS = REDS;
constexpr int WALLRANGE = REDRANGE;
constexpr int TSWALLCOLORS = GRAYS;
constexpr int FDWALLCOLORS = BROWNS;
constexpr int CDWALLCOLORS = YELLOWS;
constexpr int THINGCOLORS = GREENS;
constexpr int SECRETWALLCOLORS = WALLCOLORS;
constexpr int GRIDCOLORS = GRAYS + GRAYSRANGE / 2;
constexpr int XHAIRCOLORS = GRAYS;

// A line the map never draws, whatever the player has seen.
constexpr int LINE_NEVERSEE = ML_DONTDRAW;

// The line drawings the map is made of. Their sizes are asserted against the
// definitions in am_map.c: a count that drifts is a compile error, not a
// renderer walking off the end of an array.
constexpr int NUMPLYRLINES = 7;
constexpr int NUMCHEATPLYRLINES = 16;
constexpr int NUMTHINTRIANGLEGUYLINES = 3;

extern Doom::MapLine player_arrow[NUMPLYRLINES];
extern Doom::MapLine cheat_player_arrow[NUMCHEATPLYRLINES];
extern Doom::MapLine thintriangle_guy[NUMTHINTRIANGLEGUYLINES];

// Where the map is looking, and how far in. m_x/m_y is the lower-left corner in
// map coordinates; scale_mtof converts a map distance to a frame one.
extern fixed_t m_x, m_y;

// How much of the map the window spans, in map coordinates.
extern fixed_t m_w, m_h;

extern fixed_t scale_mtof;

// The map's rect within the frame.
extern int f_x, f_y, f_w, f_h;

// The player it draws the arrow for, and whether it is keeping them centred.
extern Doom::Player* am_plr;
extern int followplayer;

// The map cheats: `cheating` reveals the walls and the things, `grid` the grid.
extern int cheating;
extern int grid;

// Wall brightness, which the map strobes.
extern int lightlev;

// Turns a map point about the origin. The automap's own, used as it stands, so a
// renderer puts the player arrow through exactly the rotation AM_Drawer would.

// Draws the player's marks into the frame.

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
