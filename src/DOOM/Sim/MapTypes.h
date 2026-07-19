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
//      Refresh/rendering module, shared data struct definitions.
//
//-----------------------------------------------------------------------------

#pragma once

// The map's geometry as the engine holds it in memory: the vertices, sectors,
// sidedefs, linedefs, subsectors, segs and BSP nodes a level is built from, plus
// the degenerate mobj a sector uses as a sound origin. The renderer's own types
// are in Render/RenderTypes.h.

#include "../Game/GameDefs.h" // SCREENWIDTH
#include "../Math/FixedPoint.h" // fixed_t
#include "ActionFunc.h" // Thinker, for a sector's sound origin
#include "MobjTypes.h" // Mobj, which a sector holds a list of

#include <ea_data_structures/Structures/Array.h>
#include <ea_data_structures/Structures/Vector.h>

//
// INTERNAL MAP TYPES
// used by play and refresh
//

//
// Your plain vanilla vertex.
// Note: transformed values not buffered locally,
// like some DOOM-alikes ("wt", "WebView") did.
//
namespace Doom
{
struct Vertex
{
    fixed_t x;
    fixed_t y;
};
} // namespace Doom

// Forward of LineDefs, for Sectors.
namespace Doom
{
struct Line;
} // namespace Doom

// Each sector has a Doom::DegenMobj in its center
// for sound origin purposes.
// I suppose this does not handle sound from
// moving objects (doppler), because
// position is prolly just buffered, not
// updated.
// A sector's sound origin. The sound code casts it to Doom::Mobj* and reads x/y off
// it, so x/y/z must sit at the same offsets as Doom::Mobj's. Doom::Mobj inherits
// Doom::Thinker and its first field reuses the base's tail padding, so this must
// inherit it the same way rather than hold a `Doom::Thinker thinker` member - a member
// gets no tail-padding reuse, which would push x/y/z 4 bytes later than Doom::Mobj's
// and make the cast read the wrong words (a silently misplaced, wrongly-inaudible
// sound). The Thinker part is otherwise unused: this is never a real thinker.
namespace Doom
{
struct DegenMobj : Doom::Thinker
{
    fixed_t x;
    fixed_t y;
    fixed_t z;
};
} // namespace Doom

//
// The SECTORS record, at runtime.
// Stores things/mobjs.
//
namespace Doom
{
struct Sector
{
    fixed_t floorheight;
    fixed_t ceilingheight;
    short floorpic;
    short ceilingpic;
    short lightlevel;
    short special;
    short tag;

    // 0 = untraversed, 1,2 = sndlines -1
    int soundtraversed;

    // thing that made a sound (or null)
    Mobj* soundtarget;

    // mapblock bounding box for height changes
    EA::Array<int, 4> blockbox;

    // origin for any sounds played by the sector
    DegenMobj soundorg;

    // if == validcount, already checked
    int validcount;

    // list of mobjs in sector
    Mobj* thinglist;

    // Doom::Thinker for reversable actions
    void* specialdata;

    int linecount;
    struct Line** lines; // [linecount] size
};
} // namespace Doom

//
// The SideDef.
//
namespace Doom
{
struct Side
{
    // add this to the calculated texture column
    fixed_t textureoffset;

    // add this to the calculated texture top
    fixed_t rowoffset;

    // Texture indices.
    // We do not maintain names here.
    short toptexture;
    short bottomtexture;
    short midtexture;

    // Sector the SideDef is facing.
    Sector* sector;
};
} // namespace Doom

//
// Move clipping aid for LineDefs.
//
namespace Doom
{
enum SlopeType
{
    ST_HORIZONTAL,
    ST_VERTICAL,
    ST_POSITIVE,
    ST_NEGATIVE
};
} // namespace Doom

namespace Doom
{
struct Line
{
    // Vertices, from v1 to v2.
    Doom::Vertex* v1;
    Doom::Vertex* v2;

    // Precalculated v2 - v1 for side checking.
    fixed_t dx;
    fixed_t dy;

    // Animation related.
    short flags;
    short special;
    short tag;

    // Visual appearance: SideDefs.
    // sidenum[1] will be -1 if one sided
    EA::Array<short, 2> sidenum;

    // Neat. Another bounding box, for the extent
    // of the LineDef.
    EA::Array<fixed_t, 4> bbox;

    // To aid move clipping.
    SlopeType slopetype;

    // Front and back sector.
    // Note: redundant? Can be retrieved from SideDefs.
    Sector* frontsector;
    Sector* backsector;

    // if == validcount, already checked
    int validcount;

    // Doom::Thinker for reversable actions
    void* specialdata;
};
} // namespace Doom

//
// A Doom::SubSector.
// References a Doom::Sector.
// Basically, this is a list of LineSegs,
// indicating the visible walls that define
// (all or some) sides of a convex BSP leaf.
//
namespace Doom
{
struct SubSector
{
    Sector* sector;
    short numlines;
    short firstline;
};
} // namespace Doom

//
// The LineSeg.
//
namespace Doom
{
struct Seg
{
    Doom::Vertex* v1;
    Doom::Vertex* v2;

    fixed_t offset;

    angle_t angle;

    Side* sidedef;
    Line* linedef;

    // Sector references.
    // Could be retrieved from linedef, too.
    // backsector is 0 for one sided lines
    Sector* frontsector;
    Sector* backsector;
};
} // namespace Doom

//
// BSP node.
//
namespace Doom
{
struct Node
{
    // Partition line.
    fixed_t x;
    fixed_t y;
    fixed_t dx;
    fixed_t dy;

    // Bounding box for each child.
    EA::Array<EA::Array<fixed_t, 4>, 2> bbox;

    // If NF_SUBSECTOR its a subsector.
    EA::Array<unsigned short, 2> children;
};
} // namespace Doom
