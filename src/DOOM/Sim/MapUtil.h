#pragma once

#include "../doom_config.h" // doom_abs
#include "../doomdef.h"
#include "../p_local.h"
#include "../r_state.h"

#include "Clip.h"
#include "Level.h"

namespace Doom
{
// The stateful half of vanilla p_maputl: the blockmap iterators, thing-position
// linking, and the path traversal. The pure geometry (the side tests, the distance
// estimate, the line opening) lives in MapGeometry.h; this is the part that walks
// the level's blockmap grid and writes the movement scratch in Clip.
//
// p_maputl.cpp is a shim over these - it keeps the vanilla free-function names
// (P_BlockLinesIterator, P_SetThingPosition, P_PathTraverse) the rest of the still-
// vanilla engine calls, each forwarding here. Nothing here is hashed, so it is
// golden-neutral, and the demos prove it by replaying every collision, sight line
// and gunshot through it.

// Walk the lines in one blockmap cell, calling func(line_t*) for each line not
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

    for (short* list = bmap.lump + bmap.offsets[bmap.index(x, y)]; *list != -1;
         ++list)
    {
        line_t* ld = &lines[*list];

        if (ld->validcount == validcount)
            continue; // already checked from another cell

        ld->validcount = validcount;

        if (!func(ld))
            return false;
    }

    return true;
}

// Walk the things in one blockmap cell, calling func(mobj_t*) for each until one
// returns false. Same callable-taking shape as forEachLineInBlock; no validcount,
// since a thing lives in exactly one cell.
template <class ThingFunc>
bool forEachThingInBlock(int x, int y, ThingFunc&& func)
{
    const Blockmap& bmap = level().blockmap;

    if (!bmap.contains(x, y))
        return true;

    for (mobj_t* mobj = blocklinks[bmap.index(x, y)]; mobj; mobj = mobj->bnext)
        if (!func(mobj))
            return false;

    return true;
}

// Link a thing into its subsector's sector list and its blockmap cell (or neither,
// for MF_NOSECTOR / MF_NOBLOCKMAP), setting thing->subsector from its x,y.
void setThingPosition(mobj_t* thing);

// Unlink a thing from both, ahead of a position change.
void unsetThingPosition(mobj_t* thing);

// Trace the segment (x1,y1)->(x2,y2) across the blockmap, gathering the lines
// (PT_ADDLINES) and/or things (PT_ADDTHINGS) it crosses into Clip's intercept list,
// then call trav for each in near-to-far order. Returns false if any iterator or
// trav bailed early (PT_EARLYOUT hitting a solid line, a traverser saying stop).
bool pathTraverse(
    fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, int flags, traverser_t trav);
} // namespace Doom
