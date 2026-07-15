#pragma once

#include "../Math/Fixed.h"
#include "../Math/Vec2.h"

namespace Doom
{
// A directed line segment: an origin and a delta. Vanilla's divline_t is the same
// four numbers, and the sight, shooting and path-traversal code still holds one.
struct DivLine
{
    Vec2 origin;
    Vec2 delta;
};

// Which side of a line's infinite extension a point lies on: 0 in front, 1 behind.
//
// The arithmetic is vanilla's, exactly, and the shift on one factor of each
// cross-product term is load-bearing: `line.dy >> FRACBITS` keeps the 16.16
// multiply from overflowing, and it is what every recorded demo's collisions were
// computed through. It is NOT the same formula as pointOnDivlineSide below - that
// one shifts both factors by 8 and has a sign-bit fast path - and the two must not
// be merged.
inline int pointOnLineSide(Vec2 point, Vec2 lineStart, Vec2 lineDelta)
{
    if (lineDelta.x.raw == 0)
        return point.x <= lineStart.x ? lineDelta.y.raw > 0 : lineDelta.y.raw < 0;

    if (lineDelta.y.raw == 0)
        return point.y <= lineStart.y ? lineDelta.x.raw < 0 : lineDelta.x.raw > 0;

    auto d = point - lineStart;

    auto left = fixedMul(Fixed {lineDelta.y.raw >> fracBits}, d.x);
    auto right = fixedMul(d.y, Fixed {lineDelta.x.raw >> fracBits});

    return right < left ? 0 : 1;
}

// Which side of a line's infinite extension a whole bounding box lies on: 0 in
// front, 1 behind, -1 if the box straddles the line. This is what PIT_CheckLine
// asks to reject lines a mover's box cannot touch before the exact test.
//
// `slopeType` is the linedef's precomputed orientation (vanilla's slopetype_t:
// 0 ST_HORIZONTAL, 1 ST_VERTICAL, 2 ST_POSITIVE, 3 ST_NEGATIVE); the axis-aligned
// cases answer from a single edge comparison and skip the cross product, and the
// two diagonal cases test the box corners that face the line's direction. The box
// edges are the vanilla tmbox order - top/bottom/left/right. p1/p2 start at 0 so a
// slope outside 0..3 is harmless rather than undefined; the engine only ever
// passes the four it precomputed.
inline int boxOnLineSide(Fixed top,
                         Fixed bottom,
                         Fixed left,
                         Fixed right,
                         Vec2 lineStart,
                         Vec2 lineDelta,
                         int slopeType)
{
    int p1 = 0;
    int p2 = 0;

    switch (slopeType)
    {
        case 0: // ST_HORIZONTAL
            p1 = top.raw > lineStart.y.raw;
            p2 = bottom.raw > lineStart.y.raw;
            if (lineDelta.x.raw < 0)
            {
                p1 ^= 1;
                p2 ^= 1;
            }
            break;

        case 1: // ST_VERTICAL
            p1 = right.raw < lineStart.x.raw;
            p2 = left.raw < lineStart.x.raw;
            if (lineDelta.y.raw < 0)
            {
                p1 ^= 1;
                p2 ^= 1;
            }
            break;

        case 2: // ST_POSITIVE
            p1 = pointOnLineSide({left, top}, lineStart, lineDelta);
            p2 = pointOnLineSide({right, bottom}, lineStart, lineDelta);
            break;

        case 3: // ST_NEGATIVE
            p1 = pointOnLineSide({right, top}, lineStart, lineDelta);
            p2 = pointOnLineSide({left, bottom}, lineStart, lineDelta);
            break;
    }

    return p1 == p2 ? p1 : -1;
}

// The same question against a DivLine, and a different answer path: the sign-bit
// fast path decides most cases without a multiply, and when it does multiply it
// shifts BOTH factors by 8 rather than one by 16. This is what the BSP, the sight
// checks and P_PathTraverse use; the difference from pointOnLineSide is deliberate
// and demo-visible.
inline int pointOnDivlineSide(Vec2 point, const DivLine& line)
{
    if (line.delta.x.raw == 0)
        return point.x <= line.origin.x ? line.delta.y.raw > 0
                                        : line.delta.y.raw < 0;

    if (line.delta.y.raw == 0)
        return point.y <= line.origin.y ? line.delta.x.raw < 0
                                        : line.delta.x.raw > 0;

    auto d = point - line.origin;

    // Decide quickly by sign bits where that is enough.
    if ((line.delta.y.raw ^ line.delta.x.raw ^ d.x.raw ^ d.y.raw) & 0x80000000)
        return (line.delta.y.raw ^ d.x.raw) & 0x80000000 ? 1 : 0;

    auto left = fixedMul(Fixed {line.delta.y.raw >> 8}, Fixed {d.x.raw >> 8});
    auto right = fixedMul(Fixed {d.y.raw >> 8}, Fixed {line.delta.x.raw >> 8});

    return right < left ? 0 : 1;
}

// Where along `a` the two directed lines cross, as a fraction of `a`'s length.
// Parallel lines answer 0. Only the intercept traversers call it.
inline Fixed interceptVector(const DivLine& a, const DivLine& b)
{
    auto den = fixedMul(Fixed {b.delta.y.raw >> 8}, a.delta.x)
               - fixedMul(Fixed {b.delta.x.raw >> 8}, a.delta.y);

    if (den.raw == 0)
        return Fixed {0};

    auto num = fixedMul(Fixed {(b.origin.x.raw - a.origin.x.raw) >> 8}, b.delta.y)
               + fixedMul(Fixed {(a.origin.y.raw - b.origin.y.raw) >> 8}, b.delta.x);

    return fixedDiv(num, den);
}

// DOOM's cheap distance estimate: never exact, but branch-light and monotone, and
// the only distance the playsim and the renderer ever agree on. It takes the two
// axis deltas and returns |larger| + |smaller|/2 - so it overestimates a diagonal
// by up to ~12%. It is load-bearing exactly because it is this and not a real
// hypotenuse: the aim, the blockmap search radius and the sound attenuation were
// all tuned against these numbers. The halve is an arithmetic shift of the raw
// fixed value, matching vanilla's `(dx >> 1)`.
inline Fixed approxDistance(Fixed dx, Fixed dy)
{
    dx = abs(dx);
    dy = abs(dy);

    auto smaller = dx < dy ? dx : dy;
    return dx + dy - Fixed {smaller.raw >> 1};
}

// The vertical window a two-sided line leaves open: the gap between the lower of
// the two ceilings and the higher of the two floors, plus the lower floor (which a
// step-down uses). PIT_CheckLine narrows a mover's tmceilingz/tmfloorz against this
// as it contacts each line. Ties go to the back sector, exactly as vanilla's
// if/else chain does - the demos would notice min/max that rounded a tie the other
// way. The single-sided-line early-out (openrange = 0) stays with the caller,
// being about the linedef's structure rather than the two sectors' heights.
struct Opening
{
    Fixed top; // opentop:    the lower of the two ceilings
    Fixed bottom; // openbottom: the higher of the two floors
    Fixed range; // openrange:  top - bottom
    Fixed lowFloor; // lowfloor:   the lower of the two floors
};

inline Opening lineOpening(Fixed frontFloor,
                           Fixed frontCeiling,
                           Fixed backFloor,
                           Fixed backCeiling)
{
    Opening opening;

    opening.top = frontCeiling < backCeiling ? frontCeiling : backCeiling;

    if (frontFloor > backFloor)
    {
        opening.bottom = frontFloor;
        opening.lowFloor = backFloor;
    }
    else
    {
        opening.bottom = backFloor;
        opening.lowFloor = frontFloor;
    }

    opening.range = opening.top - opening.bottom;
    return opening;
}
} // namespace Doom
