#pragma once

#include "../Game/GameDefs.h" // SCREENWIDTH, SCREENHEIGHT
#include "../Math/FixedPoint.h" // fixed_t
#include "../Sim/MapTypes.h"
#include "RenderTypes.h" // VisPlane, LightTable

#include <ea_data_structures/Structures/Array.h>

namespace Doom
{
// Render/Planes' visplane and span machinery: the pool of visplanes the frame's floors and ceilings
// are batched into (visplanes / lastvisplane), the per-row silhouette scratch the plane spans are
// clipped against (openings / lastopening), the span start column, the light row for the
// current plane, and the plane-mapping cache Doom::mapPlane memoises its distance/step maths in - plus the
// cross-read plane state r_plane.h exports (floorclip / ceilingclip / yslope / distscale / lastopening),
// which lands here too so lastopening stays in the same cluster as the openings it points into.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// Render/Planes' own namespace-scope private globals, read by no other file. mapPlane, clearPlanes,
// findPlane, checkPlane, makeSpans and drawPlanes each hoist planeScratch() once and reach them
// through it, rather than through file-scope reference aliases (REFACTOR.md, Step 9 strand (a));
// lastvisplane points into visplanes, but is reset by Doom::clearPlanes each frame rather than
// bound at static-init, so it was always safe to retire. (The dead vestigial `ceilingfunc` -
// defined, never read, distinct from the shim's externed `ceilingfunc_t` - was deleted rather than
// migrated. spanstop was dropped outright in the same sweep - no reader anywhere ever set or read
// it.) Live frame-golden-covered - every floor and ceiling the demos draw is batched and mapped
// through these.
struct PlaneScratch
{
    static constexpr int maxVisplanes =
        128; // sizes visplanes; Render/Planes guards on it
    static constexpr int maxOpenings =
        SCREENWIDTH * 64; // sizes openings; Render/Planes guards on it

    EA::Array<VisPlane, maxVisplanes> visplanes =
        {}; // the frame's floor/ceiling planes
    VisPlane* lastvisplane = nullptr; // one past the last used plane
    EA::Array<short, maxOpenings> openings =
        {}; // per-column silhouette clip scratch
    EA::Array<int, SCREENHEIGHT> spanstart =
        {}; // current span's start column, per row
    LightTable** planezlight = nullptr; // light row for the current plane
    fixed_t planeheight {}; // height of the plane being mapped
    fixed_t basexscale {}; // base horizontal texture scale
    fixed_t baseyscale {}; // base vertical texture scale
    EA::Array<fixed_t, SCREENHEIGHT> cachedheight =
        {}; // Doom::mapPlane memo: plane height per row
    EA::Array<fixed_t, SCREENHEIGHT> cacheddistance = {}; // ... distance per row
    EA::Array<fixed_t, SCREENHEIGHT> cachedxstep = {}; // ... x step per row
    EA::Array<fixed_t, SCREENHEIGHT> cachedystep = {}; // ... y step per row

    // The cross-read plane state r_plane.h exports (read by Render/Segs and Render/Main),
    // bound in the r_plane.cpp shim rather than here. lastopening lives with openings so the
    // "points into a sibling array, reset by Doom::clearPlanes each frame" argument holds within
    // one cluster (a cross-cluster pointer would dangle on Engine copy).
    short* lastopening = nullptr; // next free slot in openings
    EA::Array<short, SCREENWIDTH> floorclip =
        {}; // solid pixel bounding the floor, per column
    EA::Array<short, SCREENWIDTH> ceilingclip = {}; // ... the ceiling, per column
    EA::Array<fixed_t, SCREENHEIGHT> yslope = {}; // projection y-slope, per row
    EA::Array<fixed_t, SCREENWIDTH> distscale = {}; // distance scale, per column
};

// The one PlaneScratch, a view onto the Engine's member - the same pattern as the other clusters.
PlaneScratch& planeScratch();
} // namespace Doom
