#pragma once

namespace Doom
{
// Render/Main's residual scalar: setdetail, the pending detail-mode change setViewSize
// stashes from the menu for executeSetViewSize to apply. (The drawer dispatch is its own
// cluster, Render/Drawers.h - it is the renderer's shared drawer selection, not
// Render/Main's own state.)
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); this was
// Render/Main's own namespace-scope private global, read by no other file. The vanilla name was
// a reference onto the member until the file-local-alias sweep (REFACTOR.md, Step 9 strand (a))
// retired it; setViewSize and executeSetViewSize each reach it through renderMainState()
// directly. Live frame-golden-covered. framecount, bumped once per R_RenderPlayerView, was
// deleted in a later audit: a rendered-frame tally read nowhere, in vanilla too (matching
// AutomapView::min_w/min_h and WeaponScratch::swingx/swingy).
struct RenderMainState
{
    int setdetail = 0; // pending detail-mode change from the menu
};

// The one RenderMainState, a view onto the Engine's member - the same pattern as the other clusters.
RenderMainState& renderMainState();
} // namespace Doom
