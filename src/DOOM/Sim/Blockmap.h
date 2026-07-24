#pragma once

#include "../Math/Fixed.h"
#include "../Math/Vec2.h"

namespace Doom
{
// DOOM's blockmap: a uniform grid of 128-unit cells over the whole map, so "which
// lines or things are near this point" is an array index instead of a scan. The
// engine builds it from the BLOCKMAP lump at load.
//
// This owns the descriptor - the grid's origin and extent, and the lump pointers
// the iterators read a cell's contents from - so the block-index arithmetic lives,
// and is tested, in one place rather than being re-derived at every call site
// (P_SetThingPosition, the iterators, checkPosition, P_PathTraverse, the radius
// specials). The arithmetic is vanilla's exactly: a cell coordinate is
// (world - origin) >> MAPBLOCKSHIFT, and the shift folds the /128 and the 16.16
// fixed point into one, so it is a signed arithmetic shift and negatives floor the
// way vanilla's did.
struct Blockmap
{
    // MAPBLOCKSHIFT: cells are MAPBLOCKUNITS (128 = 2^7) wide, over 16.16 fixed.
    static constexpr int shift = fracBits + 7;

    Vec2 origin; // world coords of cell (0, 0)'s corner
    int width = 0; // cells across
    int height = 0; // cells down
    short* offsets = nullptr; // per-cell offset into `lump` (vanilla `blockmap`)
    short* lump = nullptr; // the lump base the offsets index (vanilla blockmaplump)

    int blockX(Fixed x) const { return (x.raw - origin.x.raw) >> shift; }
    int blockY(Fixed y) const { return (y.raw - origin.y.raw) >> shift; }

    bool contains(int bx, int by) const
    {
        return bx >= 0 && bx < width && by >= 0 && by < height;
    }

    // The flat index of cell (bx, by) into a width*height array (blocklinks, say).
    int index(int bx, int by) const { return by * width + bx; }
};
} // namespace Doom
