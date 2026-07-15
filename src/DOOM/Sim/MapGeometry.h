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
} // namespace Doom
