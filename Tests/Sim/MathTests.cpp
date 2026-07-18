// The C++ core: Fixed, Angle, BBox, Random.
//
// PrimitiveTests.cpp pins the vanilla API - FixedMul, finesine, P_Random - and it
// still does, which is what proves the shims did not change the answers. These
// test the types underneath, which is where the behaviour actually lives now.
//
// Everything here is a pure function over data the engine ships with, so these
// boot nothing and share a process with the primitives.

#include "../Common.h"

#include <DOOM/Math/Angle.h>
#include <DOOM/Math/BBox.h>
#include <DOOM/Math/Fixed.h>
#include <DOOM/Math/Trig.h>
#include <DOOM/Sim/Random.h>

// The vanilla API as well: several of these hold the new implementation against
// the old one, which is the claim the shims make and the only thing that can
// check it.
#include <DOOM/doomtype.h>
#include <DOOM/m_bbox.h>
#include <DOOM/m_fixed.h>
#include <DOOM/m_random.h>

#include <cstddef>
#include <type_traits>

using namespace nano;
using namespace Doom;

// Everything below is in an anonymous namespace: NanoTest registers a case by
// constructing a namespace-scope variable, and two test files that happen to
// name one the same way would otherwise collide at link time.
namespace
{
constexpr auto one = Fixed::fromInt(1);
constexpr auto half = Fixed {Fixed::fracUnit / 2};

// ---------------------------------------------------------------------------
// Fixed
// ---------------------------------------------------------------------------

auto tFixedIsTrivial = test("Fixed/isATrivialWrapper") = []
{
    // It has to stay one. Half the engine still holds 16.16 numbers as bare
    // int32s in structs read straight out of the WAD, and a BBox is reinterpreted
    // over a vanilla fixed_t[4]. The moment Fixed grows a vtable or padding, all
    // of that quietly breaks.
    static_assert(sizeof(Fixed) == sizeof(std::int32_t));
    static_assert(std::is_trivially_copyable_v<Fixed>);
    static_assert(std::is_standard_layout_v<Fixed>);

    check(Fixed::fromInt(3).raw == 3 * Fixed::fracUnit);
    check(Fixed {196608}.toInt() == 3);
};

auto tFixedMulIsExact = test("Fixed/multiplyIsFixedMul") = []
{
    check(one * one == one);
    check(half * half == Fixed {Fixed::fracUnit / 4});
    check(Fixed::fromInt(3) * Fixed::fromInt(4) == Fixed::fromInt(12));
    check(-half * half == -Fixed {Fixed::fracUnit / 4});
};

// The multiply widens to 64 bits before shifting back. Doing it in 32 passes
// every small case above and then teleports objects at speed.
auto tFixedMulWidens = test("Fixed/multiplyWidensTo64Bit") = []
{ check(Fixed::fromInt(1000) * Fixed::fromInt(1000) == Fixed::fromInt(1000000)); };

auto tFixedDivSaturates = test("Fixed/divideSaturatesRatherThanOverflowing") = []
{
    check(fixedDiv(one, Fixed {1}) == Fixed {DOOM_MAXINT});
    check(fixedDiv(-one, Fixed {1}) == Fixed {DOOM_MININT});
    check(fixedDiv(one, Fixed {-1}) == Fixed {DOOM_MININT});

    check(one / Fixed::fromInt(2) == half);
    check(Fixed::fromInt(12) / Fixed::fromInt(4) == Fixed::fromInt(3));
};

// The new type must agree with the old function on every input the old function
// is ever handed, which is the whole claim the shim makes.
auto tFixedAgreesWithVanilla = test("Fixed/agreesWithVanillaFixedMul") = []
{
    for (auto a = -8; a <= 8; ++a)
    {
        for (auto b = -8; b <= 8; ++b)
        {
            auto x = Fixed {a * 12345};
            auto y = Fixed {b * 6789};

            check((x * y).raw == FixedMul(x.raw, y.raw));

            if (y.raw != 0)
                check(fixedDiv(x, y).raw == FixedDiv(x.raw, y.raw));
        }
    }
};

// ---------------------------------------------------------------------------
// Angle
// ---------------------------------------------------------------------------

auto tAngleWrapsByItself = test("Angle/wrapsWithoutAModulo") = []
{
    static_assert(sizeof(Angle) == sizeof(std::uint32_t));

    check(ang180 + ang180 == Angle {0});
    check(ang270 + ang90 == Angle {0});
    check(Angle {0} - ang90 == ang270);
    check(ang45 + ang45 == ang90);
};

auto tAngleFineIndex = test("Angle/fineIndexBucketsTheCircle") = []
{
    check(Angle {0}.fineIndex() == 0);
    check(ang90.fineIndex() == fineAngles / 4);
    check(ang180.fineIndex() == fineAngles / 2);
    check(ang270.fineIndex() == 3 * fineAngles / 4);
};

// ---------------------------------------------------------------------------
// Trig - the same quirks PrimitiveTests pins on the vanilla names, now on the
// typed accessors, so a future rewrite of a caller cannot lose them.
// ---------------------------------------------------------------------------

auto tSineIsSampledAtBucketCentres = test("Trig/sineIsSampledAtBucketCentres") = []
{
    check(fineSine(0) == Fixed {25}); // not 0
    check(fineSine(fineAngles / 4) == Fixed {65535}); // not 65536
    check(fineCosine(0) == Fixed {65535});

    check(sine(Angle {0}) == Fixed {25});
    check(cosine(ang90) == Fixed {-25});
};

auto tCosineIsSineAQuarterTurnOn = test("Trig/cosineIsSineAQuarterTurnOn") = []
{
    for (auto i = 0; i < fineAngles; ++i)
        check(fineCosine(i) == fineSine(i + fineAngles / 4));
};

auto tSlopeDivGivesUp = test("Trig/slopeDivGivesUpOnSmallDenominators") = []
{
    check(slopeDiv(0, 1) == slopeRange); // 2048, not 0
    check(slopeDiv(0, 511) == slopeRange);
    check(slopeDiv(0, 512) == 0); // at 512 it starts dividing

    check(slopeDiv(1000000, 512) == slopeRange); // never past the table
    check(slopeDiv(1, 1000000) == 0);
};

// ---------------------------------------------------------------------------
// BBox
// ---------------------------------------------------------------------------

auto tBBoxIsLayoutCompatible = test("BBox/isLayoutCompatibleWithVanilla") = []
{
    // Doom::groupLines and Doom::markRect still hand it a bare fixed_t[4], and m_bbox.cpp
    // reinterprets one as a BBox. These four are what make that legal.
    static_assert(sizeof(BBox) == 4 * sizeof(fixed_t));
    static_assert(offsetof(BBox, top) == BOXTOP * sizeof(fixed_t));
    static_assert(offsetof(BBox, bottom) == BOXBOTTOM * sizeof(fixed_t));
    static_assert(offsetof(BBox, left) == BOXLEFT * sizeof(fixed_t));
    static_assert(offsetof(BBox, right) == BOXRIGHT * sizeof(fixed_t));

    check(true);
};

// The one that matters, and the one a rewrite will want to "fix".
//
// add() is `else if`, not an independent min and max per axis. On a fresh
// (inverted) box a single point therefore moves `left` and leaves `right` at its
// sentinel - a point cannot be both below the minimum and above the maximum in
// one call. Feed it points in descending x and `right` is never written at all.
//
// min/max would be the obvious "correction" and would change what Doom::groupLines
// computes for a sector's bounding box, which changes what the renderer and
// P_BlockLinesIterator see. Vanilla's answer is the one the demos were recorded
// against.
auto tBBoxAddIsElseIf = test("BBox/addIsElseIfAndNotMinMax") = []
{
    auto box = BBox::empty();

    box.add(Fixed::fromInt(10), Fixed::fromInt(10));

    check(box.left == Fixed::fromInt(10));
    check(box.bottom == Fixed::fromInt(10));

    // Untouched: one point cannot widen both sides of an axis.
    check(box.right == Fixed {DOOM_MININT});
    check(box.top == Fixed {DOOM_MININT});

    // A second, larger point takes the other branch and the box closes up.
    box.add(Fixed::fromInt(20), Fixed::fromInt(20));

    check(box.left == Fixed::fromInt(10));
    check(box.right == Fixed::fromInt(20));
    check(box.bottom == Fixed::fromInt(10));
    check(box.top == Fixed::fromInt(20));
};

auto tBBoxMatchesVanilla = test("BBox/matchesVanillaAddToBox") = []
{
    fixed_t vanilla[4];
    M_ClearBox(vanilla);

    auto box = BBox::empty();

    const int points[][2] = {{5, 7}, {-3, 12}, {40, -8}, {1, 1}, {-100, 100}};

    for (const auto& p: points)
    {
        M_AddToBox(vanilla, Fixed::fromInt(p[0]).raw, Fixed::fromInt(p[1]).raw);
        box.add(Fixed::fromInt(p[0]), Fixed::fromInt(p[1]));
    }

    check(box.top.raw == vanilla[BOXTOP]);
    check(box.bottom.raw == vanilla[BOXBOTTOM]);
    check(box.left.raw == vanilla[BOXLEFT]);
    check(box.right.raw == vanilla[BOXRIGHT]);
};

// ---------------------------------------------------------------------------
// Random
// ---------------------------------------------------------------------------

auto tRandomStepsBeforeReading = test("Random/stepsBeforeItReads") = []
{
    auto rng = Random {};

    check(rng.playIndex == 0);
    check(rng.forPlay() == 8); // table[1], not table[0]
    check(rng.playIndex == 1);
};

auto tRandomSequenceIsTheTable = test("Random/sequenceIsTheTable") = []
{
    auto rng = Random {};

    // Every demo ever recorded depends on this exact walk.
    const int expected[] = {8, 109, 220, 222, 241, 149, 107, 75};

    for (auto value: expected)
        check(rng.forPlay() == value);
};

auto tRandomIndicesAreIndependent = test("Random/playAndMenuAreIndependent") = []
{
    auto rng = Random {};

    rng.forPlay();
    rng.forPlay();
    rng.forPlay();

    check(rng.playIndex == 3);
    check(rng.menuIndex == 0);

    rng.forMenu();

    check(rng.playIndex == 3);
    check(rng.menuIndex == 1);
};

auto tRandomWraps = test("Random/wrapsAfter256") = []
{
    auto rng = Random {};
    auto first = rng.forPlay();

    for (auto i = 1; i < 256; ++i)
        rng.forPlay();

    check(rng.playIndex == 0);
    check(rng.forPlay() == first);
};

// The legacy names are the same four bytes as the object's, which is what lets a
// rewritten caller and a vanilla one share one supply of chance.
auto tLegacyNamesAliasTheObject = test("Random/vanillaNamesAliasTheObject") = []
{
    M_ClearRandom();

    check(&prndindex == &randomness().playIndex);
    check(&rndindex == &randomness().menuIndex);

    P_Random();

    check(prndindex == 1);
    check(randomness().playIndex == 1);
};
} // namespace
