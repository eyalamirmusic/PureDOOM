#pragma once

namespace Doom
{
// validcount, the shared traversal generation counter. Every pass that must not
// process the same map element twice - the blockmap line/thing iterators, the BSP
// subsector walk, a sight check, the per-frame sector visit - bumps validcount
// once at the start, then stamps each element it visits (line->validcount =
// validcount, sec->validcount = validcount, ...) and skips any element already
// carrying the current value. r_main.cpp's "increment every time a check is made".
//
// It is genuinely engine-global: the renderer bumps it (R_RenderPlayerView,
// addSprites) and so does the playsim (P_PathTraverse, checkSight,
// checkPosition, A_LookForTargets), and neither owns it. So unlike the other
// Step-5 clusters it is not tied to a subsystem struct - it is the one scalar that
// belongs to the Engine directly, wrapped here only to keep the accessor pattern
// uniform (REFACTOR.md, Step 5). Externed as validcount in r_main.h; the vanilla
// name becomes a reference onto this member, so every reader resolves unchanged and
// the move is golden-neutral.
struct ValidCount
{
    // bumped before each traversal, stamped onto visited elements
    int validcount = 1;
};

// The one ValidCount, a view onto the Engine's member - the same pattern as
// gameClock(), renderScratch() and the rest.
ValidCount& validCount();
} // namespace Doom
