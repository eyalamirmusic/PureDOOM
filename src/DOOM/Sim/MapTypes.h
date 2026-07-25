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
#include "../Math/FixedPoint.h" // Fixed
#include "ActionFunc.h" // Thinker, for a sector's sound origin
#include "MapGeometry.h" // DivLine / Vec2, which a linedef answers questions in
#include "MobjTypes.h" // Mobj, which a sector holds a list of

#include "../Containers.h"

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
// A map vertex is a point in the plane and nothing else, so it is a Vec2 under
// the name the VERTEXES lump gives it. The alias rather than a wrapper is what
// lets a seg's or a linedef's endpoint be handed straight to the geometry
// helpers - `line.pointSide(*seg.v1)` - with no member-by-member rebuild.
using Vertex = Vec2;

// Forward of LineDefs, for Sectors.
struct Line;

// The p_spec handler enums, opaque-declared so Sector/Line below can name them in
// their method signatures without this low-level header pulling in the whole
// Thinkers/ special family (which forward-declares Sector - the reverse dependency).
// Each is a scoped enum with the default int underlying type, so the opaque
// declaration is a complete type; the real definitions live in
// Thinkers/{FloorMove,Door,Ceiling,Plat}.h and Sim/SpecialTypes.h.
enum class FloorType;
enum class StairType;
enum class DoorType;
enum class CeilingType;
enum class PlatType;
enum class MoveResult;
enum class ButtonWhere;

// Each sector has a DegenMobj in its center
// for sound origin purposes.
// I suppose this does not handle sound from
// moving objects (doppler), because
// position is prolly just buffered, not
// updated.
// A sector's sound origin. The sound code casts it to Mobj* and reads the
// position off it, so `pos` must sit at the same offset as Mobj's. Mobj inherits
// Thinker and its first field reuses the base's tail padding, so this must
// inherit it the same way rather than hold a `Thinker thinker` member - a member
// gets no tail-padding reuse, which would push pos 4 bytes later than Mobj's
// and make the cast read the wrong words (a silently misplaced, wrongly-inaudible
// sound). The Thinker part is otherwise unused: this is never a real thinker.
//
// Nothing declared before `pos`, either, for the same reason. Pinned by
// Tests/Sim/StateClusterTests.cpp - inserting a field here fails it three ways.
struct DegenMobj : Thinker
{
    Vec3 pos;
};

//
// The SECTORS record, at runtime.
// Stores things/mobjs.
//
struct Sector
{
    Fixed floorheight;
    Fixed ceilingheight;
    short floorpic = 0;
    short ceilingpic = 0;
    short lightlevel = 0;
    short special = 0;
    short tag = 0;

    // 0 = untraversed, 1,2 = sndlines -1
    int soundtraversed = 0;

    // thing that made a sound (or null)
    Mobj* soundtarget = nullptr;

    // mapblock bounding box for height changes
    Array<int, 4> blockbox;

    // origin for any sounds played by the sector
    DegenMobj soundorg;

    // if == validcount, already checked
    int validcount = 0;

    // list of mobjs in sector
    Mobj* thinglist = nullptr;

    // Thinker for reversable actions
    void* specialdata = nullptr;

    int linecount = 0;
    struct Line** lines = nullptr; // [linecount] size

    // After this sector changed height, re-clip every thing touching it; crush those
    // that no longer fit if `crunch`. Returns true if anything did not fit (vanilla
    // P_ChangeSector). Body in Sim/MapAction.cpp.
    bool changeSector(bool crunch);

    // The p_spec surrounding-sector height/light queries and the shared height-mover
    // the floor/ceiling/plat/door thinkers drive (Sim/Specials.cpp, Sim/Floors.cpp),
    // plus the per-sector light/door spawners (Sim/Lights.cpp, Sim/Doors.cpp). Each
    // is keyed off a single Sector, so each is a method.
    MoveResult movePlane(
        Fixed speed, Fixed dest, bool crush, int floorOrCeiling, int direction);
    Fixed findLowestFloorSurrounding();
    Fixed findHighestFloorSurrounding();
    Fixed findNextHighestFloor(Fixed currentheight);
    Fixed findLowestCeilingSurrounding();
    Fixed findHighestCeilingSurrounding();
    int findMinSurroundingLight(int max);
    void spawnFireFlicker();
    void spawnLightFlash();
    void spawnStrobeFlash(int fastOrSlow, int inSync);
    void spawnGlowingLight();
    void spawnDoorCloseIn30();
    void spawnDoorRaiseIn5Mins(int secnum);
};

//
// The SideDef.
//
struct Side
{
    // add this to the calculated texture column
    Fixed textureoffset;

    // add this to the calculated texture top
    Fixed rowoffset;

    // Texture indices.
    // We do not maintain names here.
    short toptexture = 0;
    short bottomtexture = 0;
    short midtexture = 0;

    // Sector the SideDef is facing.
    Sector* sector = nullptr;
};

//
// Move clipping aid for LineDefs.
//
enum class SlopeType
{
    Horizontal,
    Vertical,
    Positive,
    Negative
};

struct Line
{
    // Vertices, from v1 to v2.
    Vertex* v1 = nullptr;
    Vertex* v2 = nullptr;

    // Precalculated v2 - v1 for side checking.
    Vec2 delta;

    // Animation related.
    short flags = 0;
    short special = 0;
    short tag = 0;

    // Visual appearance: SideDefs.
    // sidenum[1] will be -1 if one sided
    Array<short, 2> sidenum;

    // Neat. Another bounding box, for the extent
    // of the LineDef.
    Array<Fixed, 4> bbox;

    // To aid move clipping.
    SlopeType slopetype = SlopeType::Horizontal;

    // Front and back sector.
    // Note: redundant? Can be retrieved from SideDefs.
    Sector* frontsector = nullptr;
    Sector* backsector = nullptr;

    // if == validcount, already checked
    int validcount = 0;

    // Thinker for reversable actions
    void* specialdata = nullptr;

    // The p_spec line-special handlers: the cross/shoot/use dispatchers and the
    // individual door/floor/ceiling/plat/light/teleport/switch effects they trigger,
    // each keyed off this Line. Bodies in Sim/{Specials,Floors,Doors,Ceilings,Plats,
    // Lights,Teleport,Switches}.cpp.
    // The linedef as a directed segment - its first vertex and the precomputed
    // v2 - v1 - which is the form the intercept traversers hand to interceptVector.
    DivLine toDivLine() const;

    // Which side of this line's infinite extension a point is on: 0 in front, 1
    // behind. This goes through pointOnLineSide and NOT pointOnDivlineSide - the
    // two are different formulae on purpose (MapGeometry.h says why), and the
    // callers depend on the specific one they ask for.
    int pointSide(Vec2 point) const;

    // The same question for a whole bounding box, in vanilla's tmbox order
    // (boxTop, boxBottom, boxLeft, boxRight): 0 in front, 1 behind, -1 straddling.
    int boxSide(const Fixed* box) const;

    // P_LineOpening: the vertical window this line leaves, written into Clip's
    // opentop / openbottom / openrange / lowfloor. A single-sided line closes it -
    // openrange = 0, and the rest of the window left as it stood, which is
    // vanilla's own early return - because a line with no back sector has no two
    // heights to compare. Bodies for the four above in Sim/MapUtil.cpp.
    void updateOpening() const;

    // The sector on the other side of this line from `sec`, or null if this line
    // does not have two of them (vanilla P_GetNextSector). Body in
    // Sim/Specials.cpp.
    Sector* nextSector(const Sector& sec) const;

    int findSectorFromLineTag(int start);
    void crossSpecialLine(int side, Mobj& thing);
    void shootSpecialLine(Mobj& thing);
    bool useSpecialLine(Mobj& thing, int side);
    int doDonut();
    int doFloor(FloorType floortype);
    int buildStairs(StairType type);
    int doDoor(DoorType type);
    int doLockedDoor(DoorType type, Mobj& thing);
    void verticalDoor(Mobj& thing);
    int doCeiling(CeilingType type);
    void activateInStasisCeiling();
    int ceilingCrushStop();
    int doPlat(PlatType type, int amount);
    void stopPlat();
    void startLightStrobing();
    void turnTagLightsOff();
    void lightTurnOn(int bright);
    int teleport(int side, Mobj& thing);
    void startButton(ButtonWhere w, int texture, int time);
    void changeSwitchTexture(int useAgain);
};

//
// A SubSector.
// References a Sector.
// Basically, this is a list of LineSegs,
// indicating the visible walls that define
// (all or some) sides of a convex BSP leaf.
//
struct SubSector
{
    Sector* sector = nullptr;
    short numlines = 0;
    short firstline = 0;
};

//
// The LineSeg.
//
struct Seg
{
    Vertex* v1 = nullptr;
    Vertex* v2 = nullptr;

    Fixed offset;

    Angle angle;

    Side* sidedef = nullptr;
    Line* linedef = nullptr;

    // Sector references.
    // Could be retrieved from linedef, too.
    // backsector is 0 for one sided lines
    Sector* frontsector = nullptr;
    Sector* backsector = nullptr;
};

//
// BSP node.
//
struct Node
{
    // The partition line. Vanilla spelled it as four loose fields and then cast
    // the whole node to a divline_t wherever it wanted them as one - the first
    // four fields being the same four numbers in the same order. It is the type
    // outright now, so the cast has nothing left to do.
    DivLine partition;

    // Bounding box for each child.
    Array<Array<Fixed, 4>, 2> bbox;

    // If NF_SUBSECTOR its a subsector.
    Array<unsigned short, 2> children;
};
} // namespace Doom
