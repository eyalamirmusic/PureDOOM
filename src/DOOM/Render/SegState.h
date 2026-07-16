#pragma once

#include "../doomtype.h" // doom_boolean
#include "../r_defs.h"   // lighttable_t

namespace Doom
{
// The wall-segment state r_segs exports for the rest of the renderer to read: whether the seg
// has a wall texture (segtextured), whether its floor/ceiling planes must be drawn (markfloor/
// markceiling, read by Render/BSP), the three chosen texture numbers (top/bottom/mid), the
// column range being drawn (rw_x/rw_stopx), the light row the drawers use (walllights, also set
// up in Render/Main), and the masked-texture column list (maskedtexturecol).
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5). These are the
// cross-read counterparts of WallScratch (which held Render/Segs' file-local intermediates):
// they are externed in r_bsp.h and read across the renderer, so their r_segs.cpp definitions and
// every extern become references onto these members. Live frame-golden-covered - every wall the
// demos draw is stored through these.
struct SegState
{
    doom_boolean segtextured = false; // the seg has a wall texture to draw
    doom_boolean markfloor = false;   // draw the floor plane for this seg
    doom_boolean markceiling = false; // draw the ceiling plane for this seg

    int toptexture = 0;    // upper texture number
    int bottomtexture = 0; // lower texture number
    int midtexture = 0;    // middle texture number

    int rw_x = 0;     // current column being drawn
    int rw_stopx = 0; // one past the last column

    lighttable_t** walllights = nullptr; // light row (a scalelight/zlight row) for the seg
    short* maskedtexturecol = nullptr;   // per-column offsets into the masked middle texture
};

SegState& segState();
} // namespace Doom
