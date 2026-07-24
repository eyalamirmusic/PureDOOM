#pragma once

#include "../Math/FixedPoint.h" // Doom::Fixed
#include "../Math/TrigTables.h" // Doom::Angle

namespace Doom
{
// The per-wall-segment rendering intermediates storeWallRange / renderSegLoop overwrite for each
// seg: whether the seg has a masked (see-through) middle texture, the seg's centre angle and texture
// offset, the scale and its per-column step, the three texture mid heights, the world-space top/
// bottom/high/low edges, and the running top/bottom fractions and their steps that walk the wall
// down the screen. Distinct from RenderScratch (the BSP-walk scratch rw_distance / rw_normalangle /
// rw_angle1 that had r_state.h externs) - these were Render/Segs' own private globals with no header
// extern.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); read by no other
// file. All twenty vanilla names were references onto the members until the file-local-alias sweep
// (REFACTOR.md, Step 9 strand (a)) retired them; Render/Segs's three functions each hoist
// wallScratch() once and read/write through it, renderSegLoop (the per-column inner loop) included -
// a per-access wallScratch() call there would be a real per-pixel cost. Live frame-golden-covered -
// every wall the demos draw is laid down through these - so the byte-identical goldens are a live
// confirmation.
struct WallScratch
{
    bool maskedtexture = false; // the seg has a masked middle texture

    Angle rw_centerangle {}; // angle from the view to the seg's centre
    Fixed rw_offset {}; // texture x offset along the seg
    Fixed rw_scale {}; // scale at the current column
    Fixed rw_scalestep {}; // per-column scale step
    Fixed rw_midtexturemid {}; // mid-texture vertical anchor
    Fixed rw_toptexturemid {}; // upper-texture vertical anchor
    Fixed rw_bottomtexturemid {}; // lower-texture vertical anchor

    Fixed worldtop {}; // seg top edge, world space
    Fixed worldbottom {}; // seg bottom edge, world space
    Fixed worldhigh {}; // back-sector ceiling edge
    Fixed worldlow {}; // back-sector floor edge

    Fixed pixhigh {}; // screen y of the upper edge
    Fixed pixlow {}; // screen y of the lower edge
    Fixed pixhighstep {}; // per-column step of pixhigh
    Fixed pixlowstep {}; // per-column step of pixlow
    Fixed topfrac {}; // running top of the wall column
    Fixed topstep {}; // per-column step of topfrac
    Fixed bottomfrac {}; // running bottom of the wall column
    Fixed bottomstep {}; // per-column step of bottomfrac
};

// The one WallScratch, a view onto the Engine's member - the same pattern as the other clusters
// (renderScratch(), compositeCache(), ...).
WallScratch& wallScratch();
} // namespace Doom
