#pragma once

#include "../Sim/MapTypes.h"
#include "RenderTypes.h" // Seg, Side, Line, Sector, DrawSeg

#include <ea_data_structures/Structures/Array.h>

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
    static constexpr int maxDrawSegs = 256; // sizes drawsegs below; the overflow
    // guards in Segs/Planes test this

    Seg* curline = nullptr; // the seg currently being drawn
    Side* sidedef = nullptr; // its sidedef
    Line* linedef = nullptr; // its linedef
    Sector* frontsector = nullptr; // the sector in front of it
    Sector* backsector = nullptr; // the sector behind it (null if one-sided)
    EA::Array<DrawSeg, maxDrawSegs> drawsegs = {}; // the frame's emitted wall ranges
    DrawSeg* ds_p = nullptr; // one past the last emitted drawseg
};

BSPScratch& bspScratch();
} // namespace Doom
