#pragma once

namespace Doom
{
// Render/Main's two residual scalars: framecount, bumped once per R_RenderPlayerView (a rendered-
// frame tally), and setdetail, the pending detail-mode change Doom::setViewSize stashes from the menu
// for Doom::executeSetViewSize to apply. (The drawer dispatch pointers colfunc/spanfunc/transcolfunc
// stay in the shim - they are the renderer's shared drawer selection, not Render/Main's own state.)
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// Render/Main's own namespace-scope private globals, read by no other file. Both vanilla names were
// references onto the members until the file-local-alias sweep (REFACTOR.md, Step 9 strand (a))
// retired them; setViewSize, executeSetViewSize, renderInit and setupFrame each reach the one member
// they touch through renderMainState() directly. Live frame-golden-covered.
struct RenderMainState
{
    int framecount = 0; // frames rendered so far
    int setdetail = 0; // pending detail-mode change from the menu
};

// The one RenderMainState, a view onto the Engine's member - the same pattern as the other clusters.
RenderMainState& renderMainState();
} // namespace Doom
