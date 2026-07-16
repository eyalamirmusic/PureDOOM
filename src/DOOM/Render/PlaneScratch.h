#pragma once

#include "../doomdef.h" // SCREENWIDTH, SCREENHEIGHT
#include "../m_fixed.h" // fixed_t
#include "../r_defs.h" // visplane_t, lighttable_t

namespace Doom
{
// Render/Planes' visplane and span machinery: the pool of visplanes the frame's floors and ceilings
// are batched into (visplanes / lastvisplane), the per-row silhouette scratch the plane spans are
// clipped against (openings, and lastopening - which stays in the r_plane shim, being header-externed
// and read by Render/Segs for masked textures), the span start/stop columns, the light row for the
// current plane, and the plane-mapping cache R_MapPlane memoises its distance/step maths in.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// Render/Planes' own namespace-scope private globals, read by no other file. The vanilla names become
// references onto the members (arrays as references-to-array). lastvisplane points into visplanes,
// but is reset by R_ClearPlanes each frame (not a self-referential initializer), so it is safe as a
// member. (The dead vestigial `ceilingfunc` - defined, never read, distinct from the shim's externed
// `ceilingfunc_t` - was deleted rather than migrated.) Live frame-golden-covered - every floor and
// ceiling the demos draw is batched and mapped through these.
struct PlaneScratch
{
    static constexpr int maxVisplanes = 128; // MAXVISPLANES in Render/Planes
    static constexpr int maxOpenings =
        SCREENWIDTH * 64; // MAXOPENINGS in Render/Planes

    visplane_t visplanes[maxVisplanes] = {}; // the frame's floor/ceiling planes
    visplane_t* lastvisplane = nullptr; // one past the last used plane
    short openings[maxOpenings] = {}; // per-column silhouette clip scratch
    int spanstart[SCREENHEIGHT] = {}; // current span's start column, per row
    int spanstop[SCREENHEIGHT] = {}; // current span's stop column, per row
    lighttable_t** planezlight = nullptr; // light row for the current plane
    fixed_t planeheight = 0; // height of the plane being mapped
    fixed_t basexscale = 0; // base horizontal texture scale
    fixed_t baseyscale = 0; // base vertical texture scale
    fixed_t cachedheight[SCREENHEIGHT] = {}; // R_MapPlane memo: plane height per row
    fixed_t cacheddistance[SCREENHEIGHT] = {}; // ... distance per row
    fixed_t cachedxstep[SCREENHEIGHT] = {}; // ... x step per row
    fixed_t cachedystep[SCREENHEIGHT] = {}; // ... y step per row
};

// The one PlaneScratch, a view onto the Engine's member - the same pattern as the other clusters.
PlaneScratch& planeScratch();
} // namespace Doom
