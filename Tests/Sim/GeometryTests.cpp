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
// Pure functions over values, so no Doom::initGame - they share the
// PrimitiveTests process.

#include "../Common.h"

#include <DOOM/Math/Fixed.h>
#include <DOOM/Math/Vec2.h>
#include <DOOM/Sim/Blockmap.h>
#include <DOOM/Sim/MapGeometry.h>

using namespace nano;
using namespace Doom;

namespace
{
constexpr Vec2 at(int x, int y)
{
    return {Fixed::fromInt(x), Fixed::fromInt(y)};
}

// boxOnLineSide with whole-unit box edges, so the cases read at a glance.
int boxSide(
    int top, int bottom, int left, int right, Vec2 start, Vec2 delta, int slope)
{
    return boxOnLineSide(Fixed::fromInt(top),
                         Fixed::fromInt(bottom),
                         Fixed::fromInt(left),
                         Fixed::fromInt(right),
                         start,
                         delta,
                         slope);
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

// approxDistance is exact on an axis, symmetric, and sign-blind (it takes abs
// first). These are the properties the blockmap search and sound attenuation rely
// on; the demos prove the shim in aggregate, these give the locality.
auto tApproxDistanceAxisAndSymmetry =
    test("Geometry/approxDistanceAxisAndSymmetry") = []
{
    check(approxDistance(Fixed::fromInt(5), Fixed {0}) == Fixed::fromInt(5),
          "distance along x is exact");
    check(approxDistance(Fixed {0}, Fixed::fromInt(5)) == Fixed::fromInt(5),
          "distance along y is exact");

    check(approxDistance(Fixed::fromInt(3), Fixed::fromInt(4))
              == approxDistance(Fixed::fromInt(4), Fixed::fromInt(3)),
          "symmetric in dx, dy");
    check(approxDistance(Fixed::fromInt(-3), Fixed::fromInt(4))
              == approxDistance(Fixed::fromInt(3), Fixed::fromInt(4)),
          "the sign of the deltas does not matter");
};

// The load-bearing part: it is |larger| + |smaller|/2, which OVERestimates a real
// diagonal. Anyone "fixing" it into a true hypotenuse changes every aim and search
// radius tuned against it and desyncs the demos.
auto tApproxDistanceOverestimatesDiagonal =
    test("Geometry/approxDistanceOverestimatesDiagonal") = []
{
    // 3-4-5: the true distance is 5, the estimate is 4 + 3/2 = 5.5.
    check(approxDistance(Fixed::fromInt(3), Fixed::fromInt(4))
              == Fixed {Fixed::fromInt(11).raw / 2},
          "3,4 estimates 5.5 units, not 5");
    // Equal legs: true 4*sqrt(2) ~= 5.66, estimate 4 + 4/2 = 6.
    check(approxDistance(Fixed::fromInt(4), Fixed::fromInt(4)) == Fixed::fromInt(6),
          "equal legs estimate 6 units");
    check(approxDistance(Fixed::fromInt(3), Fixed::fromInt(4)) > Fixed::fromInt(5),
          "the estimate never underrates the diagonal");
};

// lineOpening is the vertical window a two-sided line leaves: lower ceiling down to
// higher floor, plus the lower of the two floors. PIT_CheckLine narrows a mover
// against exactly these.
auto tLineOpeningWindow = test("Geometry/lineOpeningWindow") = []
{
    // Front floor 0 ceil 128, back floor 32 ceil 96.
    auto o = lineOpening(Fixed::fromInt(0),
                         Fixed::fromInt(128),
                         Fixed::fromInt(32),
                         Fixed::fromInt(96));

    check(o.top == Fixed::fromInt(96), "top is the lower ceiling");
    check(o.bottom == Fixed::fromInt(32), "bottom is the higher floor");
    check(o.range == Fixed::fromInt(64), "range is top - bottom");
    check(o.lowFloor == Fixed::fromInt(0), "lowFloor is the lower floor");
};

// top (min ceiling), bottom (max floor) and lowFloor (min floor) do not depend on
// which sector is called front, so swapping the pair leaves the window unchanged.
auto tLineOpeningIsSymmetric = test("Geometry/lineOpeningIsSymmetric") = []
{
    auto a = lineOpening(Fixed::fromInt(0),
                         Fixed::fromInt(128),
                         Fixed::fromInt(32),
                         Fixed::fromInt(96));
    auto b = lineOpening(Fixed::fromInt(32),
                         Fixed::fromInt(96),
                         Fixed::fromInt(0),
                         Fixed::fromInt(128));

    check(a.top == b.top && a.bottom == b.bottom && a.range == b.range
              && a.lowFloor == b.lowFloor,
          "the window is the same whichever sector is front");
};

// boxOnLineSide answers 0 (front) / 1 (back) when the whole box is on one side and
// -1 when it straddles. Front is "below an eastward line" / "east of a northward
// line", the same right-hand side pointOnLineSide uses. Each slopetype has its own
// path, so each is checked.
auto tBoxOnLineSideAxisAligned = test("Geometry/boxOnLineSideAxisAligned") = []
{
    // Eastward line along y = 0 (ST_HORIZONTAL, slope 0). Below is front.
    check(boxSide(-1, -2, -1, 1, at(0, 0), at(1, 0), 0) == 0, "box below = front");
    check(boxSide(2, 1, -1, 1, at(0, 0), at(1, 0), 0) == 1, "box above = back");
    check(boxSide(1, -1, -1, 1, at(0, 0), at(1, 0), 0) == -1, "box on it straddles");

    // Northward line along x = 0 (ST_VERTICAL, slope 1). East is front.
    check(boxSide(1, -1, 1, 2, at(0, 0), at(0, 1), 1) == 0, "box east = front");
    check(boxSide(1, -1, -2, -1, at(0, 0), at(0, 1), 1) == 1, "box west = back");
    check(boxSide(1, -1, -1, 1, at(0, 0), at(0, 1), 1) == -1, "box on it straddles");
};

auto tBoxOnLineSideDiagonal = test("Geometry/boxOnLineSideDiagonal") = []
{
    // North-east line through the origin (ST_POSITIVE, slope 2). A box in the SE
    // quadrant is clearly to its right (front); one centred on the origin straddles.
    check(boxSide(-1, -3, 1, 3, at(0, 0), at(1, 1), 2) == 0, "SE box = front");
    check(boxSide(3, 1, -3, -1, at(0, 0), at(1, 1), 2) == 1, "NW box = back");
    check(boxSide(1, -1, -1, 1, at(0, 0), at(1, 1), 2) == -1, "box on it straddles");
};

// Blockmap addressing: a world point to its 128-unit cell, the bounds check, and
// the flat index. The demos and the thing-linking scenario drive this on the
// critical path; these pin the arithmetic, including the signed shift that floors
// a point west/south of the origin into a negative (out-of-range) cell.
auto tBlockmapAddressing = test("Geometry/blockmapAddressing") = []
{
    Blockmap bm;
    bm.origin = at(-512, -256); // origin need not be at (0, 0), and usually isn't
    bm.width = 4;
    bm.height = 3;

    // 200 units east of the origin is column 1 (200 / 128 = 1.56 -> 1); 300 north
    // is row 2 (300 / 128 = 2.34 -> 2).
    check(bm.blockX(Fixed::fromInt(-512 + 200)) == 1, "200 units east = column 1");
    check(bm.blockY(Fixed::fromInt(-256 + 300)) == 2, "300 units north = row 2");
    check(bm.blockX(Fixed::fromInt(-512)) == 0
              && bm.blockY(Fixed::fromInt(-256)) == 0,
          "the origin corner is cell (0, 0)");

    // One unit before the origin floors to cell -1, out of range - the signed shift.
    check(bm.blockX(Fixed::fromInt(-513)) == -1, "west of the origin floors to -1");

    check(bm.contains(0, 0) && bm.contains(3, 2), "the corner cells are in range");
    check(!bm.contains(-1, 0) && !bm.contains(4, 0) && !bm.contains(0, 3),
          "cells outside the grid are rejected");
    check(bm.index(2, 1) == 1 * 4 + 2, "the flat index is row * width + column");
};
} // namespace
