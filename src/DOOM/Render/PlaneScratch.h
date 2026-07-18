#pragma once

#include "../doomdef.h" // SCREENWIDTH, SCREENHEIGHT
#include "../m_fixed.h" // fixed_t
#include "../Sim/MapTypes.h"
#include "RenderTypes.h" // VisPlane, LightTable

namespace Doom
{
// Render/Planes' visplane and span machinery: the pool of visplanes the frame's floors and ceilings
// are batched into (visplanes / lastvisplane), the per-row silhouette scratch the plane spans are
// clipped against (openings / lastopening), the span start/stop columns, the light row for the
// current plane, and the plane-mapping cache Doom::mapPlane memoises its distance/step maths in - plus the
// cross-read plane state r_plane.h exports (floorclip / ceilingclip / yslope / distscale / lastopening),
// which lands here too so lastopening stays in the same cluster as the openings it points into.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// Render/Planes' own namespace-scope private globals, read by no other file. The vanilla names become
// references onto the members (arrays as references-to-array). lastvisplane points into visplanes,
// but is reset by Doom::clearPlanes each frame (not a self-referential initializer), so it is safe as a
// member. (The dead vestigial `ceilingfunc` - defined, never read, distinct from the shim's externed
// `ceilingfunc_t` - was deleted rather than migrated.) Live frame-golden-covered - every floor and
// ceiling the demos draw is batched and mapped through these.
struct PlaneScratch
{
    static constexpr int maxVisplanes = 128; // MAXVISPLANES in Render/Planes
    static constexpr int maxOpenings =
        SCREENWIDTH * 64; // MAXOPENINGS in Render/Planes

    VisPlane visplanes[maxVisplanes] = {}; // the frame's floor/ceiling planes
    VisPlane* lastvisplane = nullptr; // one past the last used plane
    short openings[maxOpenings] = {}; // per-column silhouette clip scratch
    int spanstart[SCREENHEIGHT] = {}; // current span's start column, per row
    int spanstop[SCREENHEIGHT] = {}; // current span's stop column, per row
    LightTable** planezlight = nullptr; // light row for the current plane
    fixed_t planeheight = 0; // height of the plane being mapped
    fixed_t basexscale = 0; // base horizontal texture scale
    fixed_t baseyscale = 0; // base vertical texture scale
    fixed_t cachedheight[SCREENHEIGHT] = {}; // Doom::mapPlane memo: plane height per row
    fixed_t cacheddistance[SCREENHEIGHT] = {}; // ... distance per row
    fixed_t cachedxstep[SCREENHEIGHT] = {}; // ... x step per row
    fixed_t cachedystep[SCREENHEIGHT] = {}; // ... y step per row

    // The cross-read plane state r_plane.h exports (read by Render/Segs and Render/Main),
    // bound in the r_plane.cpp shim rather than here. lastopening lives with openings so the
    // "points into a sibling array, reset by Doom::clearPlanes each frame" argument holds within
    // one cluster (a cross-cluster pointer would dangle on Engine copy).
    short* lastopening = nullptr;         // next free slot in openings
    short floorclip[SCREENWIDTH] = {};    // solid pixel bounding the floor, per column
    short ceilingclip[SCREENWIDTH] = {};  // ... the ceiling, per column
    fixed_t yslope[SCREENHEIGHT] = {};    // projection y-slope, per row
    fixed_t distscale[SCREENWIDTH] = {};  // distance scale, per column
};

// The one PlaneScratch, a view onto the Engine's member - the same pattern as the other clusters.
PlaneScratch& planeScratch();
} // namespace Doom
