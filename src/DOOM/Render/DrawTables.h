#pragma once

#include "../doomtype.h" // byte

#include "../Containers.h"

namespace Doom
{
// Render/Draw's frame-address lookup tables and fuzz cursor: ylookup[y] is the framebuffer address
// of screen row y and columnofs[x] the byte offset of column x (both filled by Doom::initBuffer, so the
// column/span drawers reach a pixel by table lookup instead of a multiply), and fuzzpos is the
// spectre-fuzz drawer's running index into its distortion table.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// Render/Draw's own namespace-scope private globals, read by no other file (every apparent cross-read
// of "columnofs" is the Patch::columnofs struct member). Every drawer that touches ylookup,
// columnofs or fuzzpos - drawColumn, drawColumnLow, drawFuzzColumn, drawTranslatedColumn, drawSpan,
// drawSpanLow and Doom::initBuffer - hoists drawTables() once and reaches them through it, rather
// than through file-scope reference aliases (REFACTOR.md, Step 9 strand (a)). Live
// frame-golden-covered - every column and span the demos draw is addressed through these.
struct DrawTables
{
    static constexpr int maxHeight = 832; // sizes ylookup below
    static constexpr int maxWidth = 1120; // sizes columnofs below

    Array<byte*, maxHeight> ylookup = {}; // per-row framebuffer address
    Array<int, maxWidth> columnofs = {}; // per-column byte offset
    int fuzzpos = 0; // spectre-fuzz distortion-table cursor
};

// The one DrawTables, a view onto the Engine's member - the same pattern as the other clusters.
DrawTables& drawTables();
} // namespace Doom
