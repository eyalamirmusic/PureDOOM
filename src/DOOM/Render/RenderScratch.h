#pragma once

#include "../m_fixed.h" // fixed_t
#include "../r_defs.h" // visplane_t
#include "../tables.h" // angle_t

namespace Doom
{
// The transient state of the frame in progress, overwritten continuously as the BSP is
// walked and never persisted: the current wall segment's projection (rw_distance and
// the angles Doom::storeWallRange derives, read back by the seg loop), the floor and ceiling
// visplanes the current subsector is drawing into (Doom::subsector picks them, the plane
// code fills them), and the subsector counter. It is pure scratch - every reader writes
// it just before, so its value between frames means nothing.
//
// The sixth cluster off the loose globals into the Engine (REFACTOR.md, Step 5), and the
// last of the renderer's own per-frame state. All six were externed only in r_state.h
// with no reader outside the renderer; the storage moves off the r_segs.cpp / r_plane.cpp
// / r_main.cpp file-scope globals and the vanilla names are references onto these
// members. Nothing here is hashed (and being scratch it could not meaningfully be), so
// gathering it is golden-neutral.
struct RenderScratch
{
    // The current wall segment: its distance and the angles the seg loop reads back.
    fixed_t rw_distance = 0;
    angle_t rw_normalangle = 0;
    int rw_angle1 = 0; // angle to the line origin

    // Subsector counter, bumped as the BSP is walked.
    int sscount = 0;

    // The visplanes the current subsector draws its floor and ceiling into.
    visplane_t* floorplane = nullptr;
    visplane_t* ceilingplane = nullptr;
};

// The one RenderScratch, a view onto the Engine's member - the same pattern as
// graphicsData(), lighting(), viewWindow(), viewProjection(), viewPoint(), clip(),
// level(), wad() and randomness().
RenderScratch& renderScratch();
} // namespace Doom
