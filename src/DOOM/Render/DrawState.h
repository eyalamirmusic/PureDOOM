#pragma once

#include "../doomtype.h" // byte
#include "../Math/FixedPoint.h" // Doom::Fixed
#include "../Sim/MapTypes.h"
#include "RenderTypes.h" // LightTable

#include "../Containers.h"

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
    LightTable* dc_colormap = nullptr; // colormap row for the current column
    int dc_x = 0; // screen column being drawn
    int dc_yl = 0; // column top (inclusive)
    int dc_yh = 0; // column bottom (inclusive)
    Fixed dc_iscale {}; // inverse scale (texels per pixel)
    Fixed dc_texturemid {}; // texture vertical anchor
    byte* dc_source = nullptr; // the column's source texels
    byte* dc_translation = nullptr; // colour-translation row for sprites

    // The three player-colour translation tables, built once. translationTableStorage
    // is the backing buffer and translationTables is a 256-byte-aligned view into its
    // data(), set by initTranslationTables - which is why it is a stored pointer and
    // not translationTableStorage.data(). dc_translation indexes a colour row of it.
    Vector<byte> translationTableStorage;
    byte* translationTables = nullptr;

    int ds_y = 0; // screen row of the current span
    int ds_x1 = 0; // span start column
    int ds_x2 = 0; // span stop column
    LightTable* ds_colormap = nullptr; // colormap row for the span
    Fixed ds_xfrac {}; // texture x position
    Fixed ds_yfrac {}; // texture y position
    Fixed ds_xstep {}; // texture x step per pixel
    Fixed ds_ystep {}; // texture y step per pixel
    byte* ds_source = nullptr; // the flat's source texels
};

DrawState& drawState();
} // namespace Doom
