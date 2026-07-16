#pragma once

namespace Doom
{
// The BSP occlusion clip ranges (R_ClearClipSegs / R_ClipSolidWallSegment): as the BSP is walked
// front-to-back, solidsegs holds the sorted column spans already fully occluded by solid walls, and
// newend points one past the last valid range. A wall is drawn only where it falls outside every
// range; a solid wall then merges its span in.
//
// The cliprange_t element type moved here from Render/BSP so solidsegs could become an Engine member
// (an anonymous-struct typedef in the .cpp cannot be named in a header) - the same move MenuState
// avoided by leaving OptionsMenu file-local, done here because solidsegs *is* the state to migrate.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were Render/BSP's
// own namespace-scope private globals, read by no other file (its const checkcoord corner table stays
// file-local). The vanilla names become references onto the members (solidsegs as a reference-to-
// array). newend points into solidsegs but is set at runtime by R_ClearClipSegs, not by a self-
// referential initializer, so it is safe as a member. Live frame-golden-covered - every frame the
// demos draw walks the BSP through these.
typedef struct
{
    int first;
    int last;
} cliprange_t;

struct SolidSegs
{
    static constexpr int maxSegs = 32; // MAXSEGS in Render/BSP

    cliprange_t solidsegs[maxSegs] = {}; // sorted occluded column spans
    cliprange_t* newend = nullptr; // one past the last valid range
};

// The one SolidSegs, a view onto the Engine's member - the same pattern as the other clusters.
SolidSegs& solidSegs();
} // namespace Doom
