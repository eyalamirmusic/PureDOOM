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

// The helpers that ask MapGeometry.h's arithmetic a question about one linedef
// (toDivLine, pointSide, boxSide, updateOpening) are Line methods, declared in
// MapTypes.h; their bodies are in MapUtil.cpp.

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
    const auto& bmap = level().blockmap;

    if (!bmap.contains(x, y))
        return true;

    auto& vc = validCount();

    for (auto* list = bmap.lump + bmap.offsets[bmap.index(x, y)]; *list != -1;
         ++list)
    {
        auto* ld = &level().lines[*list];

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
    const auto& bmap = level().blockmap;

    if (!bmap.contains(x, y))
        return true;

    for (auto* mobj = level().blockLinks[bmap.index(x, y)]; mobj; mobj = mobj->bnext)
        if (!func(mobj))
            return false;

    return true;
}

// The thing-position linking (setPosition / unsetPosition) is a pair of Mobj
// methods, declared in Thinkers/Mobj.h; their bodies are in MapUtil.cpp too.

// Trace the segment (x1,y1)->(x2,y2) across the blockmap, gathering the lines
// (PT_ADDLINES) and/or things (PT_ADDTHINGS) it crosses into Clip's intercept list,
// then call trav for each in near-to-far order. Returns false if any iterator or
// trav bailed early (PT_EARLYOUT hitting a solid line, a traverser saying stop).
// The segment it walked is left in clipping().trace for the traversers to read back.
bool pathTraverse(Vec2 from, Vec2 to, int flags, Traverser trav);
} // namespace Doom
