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
//        Play functions, animation, global header.
//
//-----------------------------------------------------------------------------

#pragma once

#include "../doomtype.h"
#include "../Math/FixedPoint.h"
#include "MapTypes.h"
#include "../Render/RenderTypes.h"
#include "../Math/FunctionRef.h"
#ifndef __R_LOCAL__
#endif

namespace Doom
{
constexpr Fixed FLOATSPEED = FRACUNIT * 4;

constexpr int MAXHEALTH = 100;
constexpr Fixed VIEWHEIGHT = 41 * FRACUNIT;

// mapblocks are used to check movement
// against lines and things
constexpr int MAPBLOCKUNITS = 128;
constexpr Fixed MAPBLOCKSIZE = MAPBLOCKUNITS * FRACUNIT;
constexpr int MAPBLOCKSHIFT = fracBits + 7;
constexpr int MAPBTOFRAC = MAPBLOCKSHIFT - fracBits;

// player radius for movement checking
constexpr Fixed PLAYERRADIUS = 16 * FRACUNIT;

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
constexpr Fixed MAXRADIUS = 32 * FRACUNIT;

constexpr Fixed GRAVITY = FRACUNIT;
constexpr Fixed MAXMOVE = 30 * FRACUNIT;

constexpr Fixed USERANGE = 64 * FRACUNIT;
constexpr Fixed MELEERANGE = 64 * FRACUNIT;
constexpr Fixed MISSILERANGE = 32 * 64 * FRACUNIT;

// follow a player exlusively for 3 seconds
constexpr int BASETHRESHOLD = 100;
} // namespace Doom

//
// P_TICK
//

// both the head and tail of the thinker list; a reference onto Doom::ThinkerList's cap
// (an Engine member) - the storage moved off this loose global in Step 5

//
// P_PSPR
//

//
// P_USER
//

//
// P_MOBJ
//
namespace Doom
{
constexpr auto ONFLOORZ = Fixed {DOOM_MININT};
constexpr auto ONCEILINGZ = Fixed {DOOM_MAXINT};

// Time interval for item respawning.
constexpr int ITEMQUESIZE = 128;
} // namespace Doom

// The item respawn queue lives in Doom::ItemRespawnQueue (an Engine member) now; these are
// references onto it, the arrays as references-to-array (REFACTOR.md, Step 5).

//
// P_ENEMY
//

//
// P_MAPUTL
//
// The utilities themselves are Doom::* now - the pure geometry in
// Sim/MapGeometry.h (the side tests, the distance estimate, the line opening) and
// the stateful half in Sim/MapUtil.h (the blockmap iterators, thing linking, path
// traversal). Vanilla's divline_t went with them: it is Doom::DivLine, the same
// four numbers as an origin and a delta. What is left here is the intercept
// list's shape and the traversal flags, which the files that include p_local.h
// still name.
namespace Doom
{
struct Intercept
{
    Fixed frac; // along trace line
    bool isaline;
    union
    {
        Mobj* thing;
        Line* line;
    } d;
};
} // namespace Doom

namespace Doom
{
constexpr int MAXINTERCEPTS = 128;
} // namespace Doom
// intercepts[] and intercept_p moved into Doom::Clip (Sim/Clip.h), reached through
// Doom::clipping(); they were p_maputl's own scratch, read by no other file.

// The callback pathTraverse hands each crossed line and thing to, in near-to-far
// order. A FunctionRef rather than a bare function pointer so a traverser can be
// a lambda capturing the shot that spawned it, instead of reaching for globals.
using Traverser = Doom::FunctionRef<bool(Doom::Intercept*)>;

namespace Doom
{
constexpr int PT_ADDLINES = 1;
constexpr int PT_ADDTHINGS = 2;
constexpr int PT_EARLYOUT = 4;
} // namespace Doom

//
// P_MAP
//

// If "floatok" true, move would be ok if within "tmfloorz - tmceilingz".
// These are references into Doom::Clip now (see Sim/Movement.h); p_enemy reads
// floatok/tmfloorz, p_mobj reads ceilingline.

//
// P_SETUP
//
// The blockmap and reject views (blockmap/blockmaplump/bmapwidth/bmapheight/
// bmaporgx/bmaporgy/blocklinks/rejectmatrix) were here. They are read off
// Level::blockmap, Level::blockLinks and Level::rejectMatrix now - the descriptor
// that already owned the same numbers.

//
// P_INTER
//

//
// P_SPEC
//
#include "SpecialTypes.h"

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
