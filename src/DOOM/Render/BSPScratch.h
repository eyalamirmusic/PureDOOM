#pragma once

#include "../r_defs.h" // seg_t, side_t, line_t, sector_t, drawseg_t

namespace Doom
{
// Render/BSP's traversal state: the current seg being drawn and the sidedef, linedef and
// front/back sectors it belongs to (set as Doom::subsector / Doom::addLine walk the tree), plus the
// drawseg pool the walk emits - one drawseg per solid or masked wall range (drawsegs), with
// ds_p one past the last. Render/Segs and Render/Things read the drawsegs back for
// masked-texture and sprite clipping.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// externed in r_bsp.h and read across the renderer. The vanilla names become references onto
// the members (drawsegs as a reference-to-array). ds_p points into drawsegs but is reset to it
// by Doom::clearDrawSegs each frame, not by a self-referential initializer, so it is safe as a
// member (the SolidSegs / PlaneScratch precedent). Live frame-golden-covered - every wall the
// demos draw is emitted through these.
struct BSPScratch
{
    static constexpr int maxDrawSegs = 256; // MAXDRAWSEGS in r_defs.h

    seg_t* curline = nullptr;        // the seg currently being drawn
    side_t* sidedef = nullptr;       // its sidedef
    line_t* linedef = nullptr;       // its linedef
    sector_t* frontsector = nullptr; // the sector in front of it
    sector_t* backsector = nullptr;  // the sector behind it (null if one-sided)
    drawseg_t drawsegs[maxDrawSegs] = {}; // the frame's emitted wall ranges
    drawseg_t* ds_p = nullptr;       // one past the last emitted drawseg
};

BSPScratch& bspScratch();
} // namespace Doom
