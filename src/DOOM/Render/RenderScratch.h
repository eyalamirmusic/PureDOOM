#pragma once

#include "../Math/FixedPoint.h" // Doom::Fixed
#include "../Sim/MapTypes.h"
#include "RenderTypes.h" // VisPlane
#include "../Math/TrigTables.h" // Doom::Angle

namespace Doom
{
// The transient state of the frame in progress, overwritten continuously as the BSP is
// walked and never persisted: the current wall segment's projection (rw_distance and
// the angles storeWallRange derives, read back by the seg loop), and the floor and ceiling
// visplanes the current subsector is drawing into (subsector picks them, the plane
// code fills them). It is pure scratch - every reader writes
// it just before, so its value between frames means nothing.
//
// The sixth cluster off the loose globals into the Engine (REFACTOR.md, Step 5), and the
// last of the renderer's own per-frame state. All were externed only in r_state.h
// with no reader outside the renderer; the storage moves off the r_segs.cpp / r_plane.cpp
// / r_main.cpp file-scope globals and the vanilla names are references onto these
// members. Nothing here is hashed (and being scratch it could not meaningfully be), so
// gathering it is golden-neutral. sscount, the subsector counter subsector bumped,
// was deleted in a later audit: incremented once a subsector but read nowhere, in vanilla
// too (matching AutomapView::min_w/min_h and WeaponScratch::swingx/swingy).
struct RenderScratch
{
    // The current wall segment: its distance and the angles the seg loop reads back.
    Fixed rw_distance {};
    Angle rw_normalangle {};
    Angle rw_angle1 {}; // angle to the line origin

    // The visplanes the current subsector draws its floor and ceiling into.
    VisPlane* floorplane = nullptr;
    VisPlane* ceilingplane = nullptr;
};

// The one RenderScratch, a view onto the Engine's member - the same pattern as
// graphicsData(), lighting(), viewWindow(), viewProjection(), viewPoint(), clipping(),
// level(), wad() and randomness().
RenderScratch& renderScratch();
} // namespace Doom
