#pragma once

#include "../Host/Platform.h" // doom_abs
#include "../Game/GameDefs.h"
#include "SimDefs.h"

#include "Clip.h"
#include "Level.h"
#include "MapGeometry.h"
#include "ValidCount.h"

#include "../Math/BBox.h"
namespace Doom
{
// Vanilla p_maputl, in full: the blockmap iterators, thing-position linking, the
// path traversal, and the handful of helpers that ask MapGeometry.h's pure
// arithmetic a question about a linedef. The pure arithmetic itself (the side
// tests, the distance estimate, the line opening) is in MapGeometry.h; this is the
// part that knows about the level's blockmap grid and the movement scratch in Clip.
//
// Nothing here is hashed, so it is golden-neutral, and the demos prove it by
// replaying every collision, sight line and gunshot through it.

// The linedef as a directed segment - its first vertex and the precomputed
// v2 - v1 - which is the form the intercept traversers hand to interceptVector.
DivLine makeDivLine(const Line& line);

// Which side of a linedef's infinite extension a point is on: 0 in front, 1
// behind. This goes through pointOnLineSide and NOT pointOnDivlineSide - the two
// are different formulae on purpose (MapGeometry.h says why), and the callers
// depend on the specific one they ask for.
int lineSide(Vec2 point, const Line& line);

// The same question for a whole bounding box, in vanilla's tmbox order
// (BOXTOP, BOXBOTTOM, BOXLEFT, BOXRIGHT): 0 in front, 1 behind, -1 straddling.
int boxLineSide(const fixed_t* box, const Line& line);

// P_LineOpening: the vertical window the line leaves, written into Clip's
// opentop / openbottom / openrange / lowfloor. A single-sided line closes it -
// openrange = 0, and the rest of the window left as it stood, which is vanilla's
// own early return - because a line with no back sector has no two heights to
// compare.
void updateLineOpening(const Line& linedef);

// Walk the lines in one blockmap cell, calling func(Line*) for each line not
// already seen this validcount, and stop early the moment func returns false.
//
// The template takes any callable, which is the point: a rewritten caller passes a
// lambda that captures its own clip state where vanilla could only pass a bare
// PIT_* function pointer through a global. The validcount de-dup is vanilla's - a
// line spanning several cells is checked once per traverse - so the caller must
// bump validcount before the first call, exactly as before.
template <class LineFunc>
bool forEachLineInBlock(int x, int y, LineFunc&& func)
{
    const Blockmap& bmap = level().blockmap;

    if (!bmap.contains(x, y))
        return true;

    auto& vc = validCount();

    for (short* list = bmap.lump + bmap.offsets[bmap.index(x, y)]; *list != -1;
         ++list)
    {
        Line* ld = &lines[*list];

        if (ld->validcount == vc.validcount)
            continue; // already checked from another cell

        ld->validcount = vc.validcount;

        if (!func(ld))
            return false;
    }

    return true;
}

// Walk the things in one blockmap cell, calling func(Mobj*) for each until one
// returns false. Same callable-taking shape as forEachLineInBlock; no validcount,
// since a thing lives in exactly one cell.
template <class ThingFunc>
bool forEachThingInBlock(int x, int y, ThingFunc&& func)
{
    const Blockmap& bmap = level().blockmap;

    if (!bmap.contains(x, y))
        return true;

    for (Mobj* mobj = blocklinks[bmap.index(x, y)]; mobj; mobj = mobj->bnext)
        if (!func(mobj))
            return false;

    return true;
}

// Link a thing into its subsector's sector list and its blockmap cell (or neither,
// for MF_NOSECTOR / MF_NOBLOCKMAP), setting thing.subsector from its x,y.
void setThingPosition(Mobj& thing);

// Unlink a thing from both, ahead of a position change.
void unsetThingPosition(Mobj& thing);

// Trace the segment (x1,y1)->(x2,y2) across the blockmap, gathering the lines
// (PT_ADDLINES) and/or things (PT_ADDTHINGS) it crosses into Clip's intercept list,
// then call trav for each in near-to-far order. Returns false if any iterator or
// trav bailed early (PT_EARLYOUT hitting a solid line, a traverser saying stop).
// The segment it walked is left in clip().trace for the traversers to read back.
bool pathTraverse(
    fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, int flags, Traverser trav);
} // namespace Doom
