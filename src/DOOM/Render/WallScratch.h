#pragma once

#include "../doomtype.h" // doom_boolean
#include "../Math/FixedPoint.h" // fixed_t
#include "../Math/TrigTables.h" // angle_t

namespace Doom
{
// The per-wall-segment rendering intermediates Doom::storeWallRange / Doom::renderSegLoop overwrite for each
// seg: whether the seg has a masked (see-through) middle texture, the seg's centre angle and texture
// offset, the scale and its per-column step, the three texture mid heights, the world-space top/
// bottom/high/low edges, and the running top/bottom fractions and their steps that walk the wall
// down the screen. Distinct from RenderScratch (the BSP-walk scratch rw_distance / rw_normalangle /
// rw_angle1 that had r_state.h externs) - these were Render/Segs' own private globals with no header
// extern.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); read by no other
// file. The vanilla names become references onto the members. Live frame-golden-covered - every wall
// the demos draw is laid down through these - so the byte-identical goldens are a live confirmation.
struct WallScratch
{
    doom_boolean maskedtexture = false; // the seg has a masked middle texture

    angle_t rw_centerangle = 0; // angle from the view to the seg's centre
    fixed_t rw_offset = 0; // texture x offset along the seg
    fixed_t rw_scale = 0; // scale at the current column
    fixed_t rw_scalestep = 0; // per-column scale step
    fixed_t rw_midtexturemid = 0; // mid-texture vertical anchor
    fixed_t rw_toptexturemid = 0; // upper-texture vertical anchor
    fixed_t rw_bottomtexturemid = 0; // lower-texture vertical anchor

    int worldtop = 0; // seg top edge, world space
    int worldbottom = 0; // seg bottom edge, world space
    int worldhigh = 0; // back-sector ceiling edge
    int worldlow = 0; // back-sector floor edge

    fixed_t pixhigh = 0; // screen y of the upper edge
    fixed_t pixlow = 0; // screen y of the lower edge
    fixed_t pixhighstep = 0; // per-column step of pixhigh
    fixed_t pixlowstep = 0; // per-column step of pixlow
    fixed_t topfrac = 0; // running top of the wall column
    fixed_t topstep = 0; // per-column step of topfrac
    fixed_t bottomfrac = 0; // running bottom of the wall column
    fixed_t bottomstep = 0; // per-column step of bottomfrac
};

// The one WallScratch, a view onto the Engine's member - the same pattern as the other clusters
// (renderScratch(), compositeCache(), ...).
WallScratch& wallScratch();
} // namespace Doom
