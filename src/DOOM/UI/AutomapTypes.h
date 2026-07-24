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
// Everything this header declares is at global scope, not in namespace Doom:
// examples/EACP/EngineAccess.cpp is an ordinary translation unit and reads these
// by bare name. Keep new declarations here at :: scope for the same reason.
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
    Fixed x, y;
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
constexpr int LINE_NEVERSEE = Doom::ML_DONTDRAW;

// The line drawings the map is made of. These counts are the array bounds the
// tables in UI/Automap.cpp are defined against, so a count that drifts from its
// table is a compile error rather than a renderer walking off the end of it.
constexpr int NUMPLYRLINES = 7;
constexpr int NUMCHEATPLYRLINES = 16;
constexpr int NUMTHINTRIANGLEGUYLINES = 3;

namespace Doom
{
// The three line drawings, immutable after static init - the player arrow, the
// IDDT arrow, and the shape every *thing* is drawn as. Const, and reached through
// mapShapes(), so the compositor in examples/EACP reads the same tables the
// software automap draws from without either of them being a mutable global.
struct MapShapes
{
    Array<MapLine, NUMPLYRLINES> playerArrow;
    Array<MapLine, NUMCHEATPLYRLINES> cheatPlayerArrow;
    Array<MapLine, NUMTHINTRIANGLEGUYLINES> thinTriangleGuy;
};

const MapShapes& mapShapes();
} // namespace Doom

// The map window's position, extent, scale and frame rect, the player it follows,
// and the two cheats, all used to be twelve loose globals here. They are members
// of AutomapView (UI/AutomapView.h) now - the automap's own state cluster, which
// already held the rest of the same state - and are reached through
// Doom::automapView().

// Turns a map point about the origin. The automap's own, used as it stands, so a
// renderer puts the player arrow through exactly the rotation AM_Drawer would.

// Draws the player's marks into the frame.

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
