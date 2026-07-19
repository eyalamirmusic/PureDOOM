#pragma once

#include "../doomtype.h" // byte

namespace Doom
{
// Render/Draw's frame-address lookup tables and fuzz cursor: ylookup[y] is the framebuffer address
// of screen row y and columnofs[x] the byte offset of column x (both filled by Doom::initBuffer, so the
// column/span drawers reach a pixel by table lookup instead of a multiply), and fuzzpos is the
// spectre-fuzz drawer's running index into its distortion table.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// Render/Draw's own namespace-scope private globals, read by no other file (every apparent cross-read
// of "columnofs" is the Patch::columnofs struct member). ylookup and columnofs are references onto
// the members (the tables as references-to-array); fuzzpos was a reference too until the
// file-local-alias sweep (REFACTOR.md, Step 9 strand (a)) retired it - drawFuzzColumn, its only
// toucher, hoists drawTables() once. Live frame-golden-covered - every column and span the demos
// draw is addressed through these.
struct DrawTables
{
    static constexpr int maxHeight = 832; // MAXHEIGHT in Render/Draw
    static constexpr int maxWidth = 1120; // MAXWIDTH in Render/Draw

    byte* ylookup[maxHeight] = {}; // per-row framebuffer address
    int columnofs[maxWidth] = {}; // per-column byte offset
    int fuzzpos = 0; // spectre-fuzz distortion-table cursor
};

// The one DrawTables, a view onto the Engine's member - the same pattern as the other clusters.
DrawTables& drawTables();
} // namespace Doom
