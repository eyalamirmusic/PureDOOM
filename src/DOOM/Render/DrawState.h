#pragma once

#include "../doomtype.h" // byte
#include "../m_fixed.h"  // fixed_t
#include "../r_defs.h"   // lighttable_t

namespace Doom
{
// The column and span drawer parameters r_draw exports: the callers (Render/Segs, Render/Planes,
// Render/Things) fill these in and then call colfunc()/spanfunc(), which read them. The dc_* set
// describe one texture column (its colormap, screen column and top/bottom, inverse scale, texture
// anchor and source pixels, plus the translation table for coloured sprites); the ds_* set
// describe one flat span (its row, column range, colormap, texture position/step and source).
// translationtables is the once-built table dc_translation indexes a colour row of.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these are the
// cross-read drawer inputs (the counterpart of DrawTables, Render/Draw's file-local ylookup/
// columnofs/fuzzpos). All are externed in r_draw.h; their r_draw.cpp definitions and those externs
// become references onto these members. dc_translation points into translationtables but is set at
// runtime, not by a self-referential initializer. Live frame-golden-covered - every column and
// span the demos draw is set up through these.
struct DrawState
{
    lighttable_t* dc_colormap = nullptr; // colormap row for the current column
    int dc_x = 0;                        // screen column being drawn
    int dc_yl = 0;                       // column top (inclusive)
    int dc_yh = 0;                       // column bottom (inclusive)
    fixed_t dc_iscale = 0;               // inverse scale (texels per pixel)
    fixed_t dc_texturemid = 0;           // texture vertical anchor
    byte* dc_source = nullptr;           // the column's source texels
    byte* dc_translation = nullptr;      // colour-translation row for sprites
    byte* translationtables = nullptr;   // the three player-colour translation tables

    int ds_y = 0;                        // screen row of the current span
    int ds_x1 = 0;                       // span start column
    int ds_x2 = 0;                       // span stop column
    lighttable_t* ds_colormap = nullptr; // colormap row for the span
    fixed_t ds_xfrac = 0;                // texture x position
    fixed_t ds_yfrac = 0;                // texture y position
    fixed_t ds_xstep = 0;                // texture x step per pixel
    fixed_t ds_ystep = 0;                // texture y step per pixel
    byte* ds_source = nullptr;           // the flat's source texels
};

DrawState& drawState();
} // namespace Doom
