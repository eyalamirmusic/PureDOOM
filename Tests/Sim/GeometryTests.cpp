// The map geometry the playsim is built on: which side of a line a point is on,
// and where two lines cross. Sim/MapGeometry.h.
//
// The demos pin these in aggregate - a wrong side flips a collision and desyncs
// within tics - and prove the vanilla P_* shims extract the right fields (a
// bit-identical replay could not survive reading dx where dy was meant). What the
// demos cannot give is locality: these say which of the three functions is wrong,
// and they pin the two quirks that a "cleanup" would flatten - pointOnLineSide and
// pointOnDivlineSide are different formulae and must stay different.
//
// Pure functions over values, so no doom_init - they share the PrimitiveTests
// process.

#include "../Common.h"

#include <DOOM/Math/Fixed.h>
#include <DOOM/Math/Vec2.h>
#include <DOOM/Sim/MapGeometry.h>

using namespace nano;
using namespace Doom;

namespace
{
constexpr Vec2 at(int x, int y)
{
    return {Fixed::fromInt(x), Fixed::fromInt(y)};
}

// side 0 is "front" - to the right of the line's direction (the right-hand rule
// DOOM's linedefs follow). These cases are chosen so the answer is obvious from
// the geometry, not read back off the implementation.

auto tPointOnLineSideCardinals = test("Geometry/pointOnLineSideByHand") = []
{
    // A line pointing east from the origin. Its front (right) is to the south.
    check(pointOnLineSide(at(0, -1), at(0, 0), at(1, 0)) == 0,
          "south of east = front");
    check(pointOnLineSide(at(0, 1), at(0, 0), at(1, 0)) == 1,
          "north of east = back");

    // Pointing north: front is to the east.
    check(pointOnLineSide(at(1, 0), at(0, 0), at(0, 1)) == 0,
          "east of north = front");
    check(pointOnLineSide(at(-1, 0), at(0, 0), at(0, 1)) == 1,
          "west of north = back");
};

auto tPointOnLineSideDiagonal = test("Geometry/pointOnLineSideDiagonal") = []
{
    // A line heading north-east. A point due east of the start is to its right.
    check(pointOnLineSide(at(1, 0), at(0, 0), at(1, 1)) == 0, "east of NE = front");
    check(pointOnLineSide(at(0, 1), at(0, 0), at(1, 1)) == 1, "north of NE = back");
};

// The divline formula is a different path - a sign-bit fast path, then a both-
// factors-shifted-by-8 multiply - but on unambiguous geometry it must reach the
// same side pointOnLineSide does. The point of pinning both is that they stay
// separate; the point of pinning agreement is that neither drifted.
auto tDivlineSideAgreesOnClearCases =
    test("Geometry/divlineSideAgreesOnClearCases") = []
{
    const Vec2 starts[] = {at(0, 0), at(5, -3), at(-8, 2)};
    const Vec2 deltas[] = {at(1, 0), at(0, 1), at(1, 1), at(-2, 1), at(3, -4)};
    const Vec2 offsets[] = {at(4, 7), at(-6, 5), at(9, -8), at(-3, -10)};

    for (auto start: starts)
        for (auto delta: deltas)
            for (auto offset: offsets)
            {
                auto point = start + offset;
                auto line = pointOnLineSide(point, start, delta);
                auto div = pointOnDivlineSide(point, {start, delta});

                // They may differ exactly on the line; these offsets are chosen to
                // sit clearly off it, so they must agree.
                check(line == div, "line and divline side agree off the line");
            }
};

auto tInterceptVectorCrossing = test("Geometry/interceptVectorFindsTheCrossing") = []
{
    // `a` runs east from the origin, length 4. `b` runs north through x = 1.
    // They cross at (1, 0), a quarter of the way along a.
    auto a = DivLine {at(0, 0), at(4, 0)};
    auto b = DivLine {at(1, -1), at(0, 2)};

    auto frac = interceptVector(a, b);

    // A quarter of FRACUNIT, give or take the >>8 truncation in the formula.
    check(abs(frac - Fixed {Fixed::fracUnit / 4}).raw < 64,
          "crosses at ~1/4 along a");
};

auto tInterceptVectorParallel = test("Geometry/interceptVectorParallelIsZero") = []
{
    auto a = DivLine {at(0, 0), at(1, 0)};
    auto b = DivLine {at(0, 5), at(1, 0)};

    check(interceptVector(a, b) == Fixed {0}, "parallel lines answer zero");
};
} // namespace
