#pragma once

namespace Doom
{
// Render/Main's two residual scalars: framecount, bumped once per R_RenderPlayerView (a rendered-
// frame tally), and setdetail, the pending detail-mode change R_SetViewSize stashes from the menu
// for R_ExecuteSetViewSize to apply. (The drawer dispatch pointers colfunc/spanfunc/transcolfunc
// stay in the shim - they are the renderer's shared drawer selection, not Render/Main's own state.)
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// Render/Main's own namespace-scope private globals, read by no other file. The vanilla names become
// references onto the members. Live frame-golden-covered.
struct RenderMainState
{
    int framecount = 0; // frames rendered so far
    int setdetail = 0; // pending detail-mode change from the menu
};

// The one RenderMainState, a view onto the Engine's member - the same pattern as the other clusters.
RenderMainState& renderMainState();
} // namespace Doom
