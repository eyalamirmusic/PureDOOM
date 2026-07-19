#pragma once

#include "Fixed.h"

#include <algorithm>
#include <limits>

namespace Doom
{
// An axis-aligned bounding box, as the blockmap, the BSP and every collision
// check use one.
//
// Vanilla spells this as a bare fixed_t[4] indexed by BOXTOP/BOXBOTTOM/BOXLEFT/
// BOXRIGHT, which is why so much of the engine passes bounding boxes around as
// naked pointers. It is the same four numbers, in the same order, so a BBox and a
// vanilla box are layout-compatible - which is what lets the two coexist while
// the engine is only half rewritten.
struct BBox
{
    Fixed top;
    Fixed bottom;
    Fixed left;
    Fixed right;

    // Empty, and empty in the way the engine means it: inverted, so that the
    // first point added to it defines the box on every axis at once.
    static constexpr BBox empty()
    {
        constexpr auto lowest = std::numeric_limits<std::int32_t>::min();
        constexpr auto highest = std::numeric_limits<std::int32_t>::max();

        return {Fixed {lowest}, Fixed {highest}, Fixed {highest}, Fixed {lowest}};
    }

    // `else if`, and NOT an independent min and max on each axis, which is what
    // this obviously wants to be and must not become.
    //
    // On an inverted box the two are not the same. Add a single point to a fresh
    // box and vanilla moves `left` and leaves `right` at its sentinel, because
    // the point cannot be both below the minimum and above the maximum in one
    // call. Add points in descending x and `right` is never written at all. The
    // engine gets away with it - Doom::groupLines feeds it whole linedefs, and
    // Doom::markRect feeds it a top-left then a bottom-right - but "gets away with"
    // is not "does not depend on", and min/max here changes what Doom::groupLines
    // computes for a sector's bounding box, which changes what the renderer and
    // P_BlockLinesIterator see.
    //
    // Pinned by Tests/Sim/MathTests.cpp.
    constexpr void add(Fixed x, Fixed y)
    {
        if (x < left)
            left = x;
        else if (x > right)
            right = x;

        if (y < bottom)
            bottom = y;
        else if (y > top)
            top = y;
    }
};

// The vanilla index names for a bare fixed_t[4] box, and the bridge onto BBox.
//
// Vanilla passes boxes around as a fixed_t[4] indexed by these, and half the engine
// still holds them that way - a sector's bbox, a linedef's, the blockmap's. (The
// element type is spelled std::int32_t rather than the vanilla fixed_t so Math/ stays
// free of the vanilla headers; they are the same type.) BBox has
// the same four numbers in the same order (asserted in BBox.cpp), so the bridge can
// reinterpret rather than copy. Scaffolding: it goes when the last fixed_t[4] does.
enum
{
    BOXTOP,
    BOXBOTTOM,
    BOXLEFT,
    BOXRIGHT
};

BBox& asBBox(Fixed* box);
void clearBox(Fixed* box);
void addToBox(Fixed* box, Fixed x, Fixed y);
} // namespace Doom
