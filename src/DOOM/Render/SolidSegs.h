#pragma once

#include "../Containers.h"

namespace Doom
{
// The BSP occlusion clip ranges (Doom::clearClipSegs / Doom::clipSolidWallSegment): as the BSP is walked
// front-to-back, solidsegs holds the sorted column spans already fully occluded by solid walls, and
// newend points one past the last valid range. A wall is drawn only where it falls outside every
// range; a solid wall then merges its span in.
//
// The ClipRange element type moved here from Render/BSP so solidsegs could become an Engine member
// (an anonymous-struct typedef in the .cpp cannot be named in a header) - the same move MenuState
// avoided by leaving OptionsMenu file-local, done here because solidsegs *is* the state to migrate.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were Render/BSP's
// own namespace-scope private globals, read by no other file (its const checkcoord corner table stays
// file-local). solidsegs and newend were both references onto the member (solidsegs a
// reference-to-array; newend points into solidsegs but is set at runtime by Doom::clearClipSegs, not
// by a self-referential initializer, so it was safe as one) until the file-local-alias sweep
// (REFACTOR.md, Step 9 strand (a)) retired them; clipSolidWallSegment, clipPassWallSegment,
// clearClipSegs and checkBBox each hoist solidSegs() once and reach its members through it. Live
// frame-golden-covered - every frame the demos draw walks the BSP through these.
struct ClipRange
{
    int first;
    int last;
};

struct SolidSegs
{
    static constexpr int maxSegs = 32; // sizes solidsegs below

    Array<ClipRange, maxSegs> solidsegs = {}; // sorted occluded column spans
    ClipRange* newend = nullptr; // one past the last valid range
};

// The one SolidSegs, a view onto the Engine's member - the same pattern as the other clusters.
SolidSegs& solidSegs();
} // namespace Doom
