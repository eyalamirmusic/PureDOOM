// The arithmetic the simulation is built out of.
//
// The demo tests say *whether* the world changed. These say *what* changed: when
// a demo desyncs at tic 48, the useful question is which primitive stopped
// agreeing with itself, and a failing case in here answers it directly. They
// need no WAD and no doom_init - every one of these is a pure function over a
// table the engine ships with - so they run in microseconds and can all share a
// process.

#include "../Common.h"

// No extern "C" here any more: the engine is C++ as of Step 2 of REFACTOR.md, so
#include <DOOM/Math/Trig.h>
// these are C++ declarations of C++ functions and wrapping them would ask the
// linker for symbols that do not exist.
#include <DOOM/doomtype.h>
#include <DOOM/info.h>
#include <DOOM/m_fixed.h>
#include <DOOM/m_random.h>
#include <DOOM/r_main.h>
#include <DOOM/tables.h>

fixed_t P_AproxDistance(fixed_t dx, fixed_t dy);

#include <cstdint>

#include <DOOM/Render/Main.h>
using namespace nano;

namespace
{
constexpr auto one = FRACUNIT;
constexpr auto half = FRACUNIT / 2;

// FNV-1a, the same mix the simulation probe uses, so a checksum quoted in one
// place means the same thing in the other.
struct Checksum
{
    std::uint64_t value = 1469598103934665603ULL;

    Checksum& operator<<(std::int64_t datum)
    {
        for (auto i = 0; i < 8; ++i)
        {
            value ^= (std::uint64_t) ((datum >> (i * 8)) & 0xff);
            value *= 1099511628211ULL;
        }

        return *this;
    }
};

// Says what the table now checksums to, not merely that it disagrees. A table is
// 16,000 numbers and "false is not true" would leave you with nothing to do next.
void checkTable(const char* table, const Checksum& sum, std::uint64_t expected)
{
    if (sum.value != expected)
        std::printf("\n%s has changed.\n"
                    "  expected 0x%016llx\n"
                    "  now      0x%016llx\n"
                    "  If the table was rewritten and this was intended, put the\n"
                    "  new checksum in PrimitiveTests.cpp - on purpose, and having\n"
                    "  looked at what moved.\n\n",
                    table,
                    (unsigned long long) expected,
                    (unsigned long long) sum.value);

    check(sum.value == expected, "the table is intact");
}
} // namespace

// ---------------------------------------------------------------------------
// Fixed point. Every position, velocity and height in DOOM is one of these:
// a 32-bit integer holding 16 bits of fraction.
// ---------------------------------------------------------------------------

auto tFixedMulIdentity = test("Fixed/mulByOneIsIdentity") = []
{
    check(FixedMul(12345, one) == 12345);
    check(FixedMul(one, 12345) == 12345);
    check(FixedMul(-12345, one) == -12345);
};

auto tFixedMulHalves = test("Fixed/mulHalves") = []
{
    check(FixedMul(half, half) == FRACUNIT / 4);
    check(FixedMul(-half, half) == -(FRACUNIT / 4));
    check(FixedMul(3 * one, 4 * one) == 12 * one);
};

// The multiply widens to 64 bits before shifting back down, so a product that
// would overflow 32 bits mid-way still comes out right. A refactor that does the
// shift in 32 bits would pass every small case above and break this one - and
// would then only show up in-game as objects teleporting at high speed.
auto tFixedMulDoesNotOverflowMidway = test("Fixed/mulWidensTo64Bit") = []
{ check(FixedMul(1000 * one, 1000 * one) == 1000000 * one); };

auto tFixedDivIdentity = test("Fixed/divByOneIsIdentity") = []
{
    check(FixedDiv(12345, one) == 12345);
    check(FixedDiv(one, one) == one);
};

auto tFixedDivHalves = test("Fixed/divHalves") = []
{
    check(FixedDiv(one, 2 * one) == half);
    check(FixedDiv(12 * one, 4 * one) == 3 * one);
};

// FixedDiv's guard, and the one piece of it that is easy to get wrong. When the
// quotient will not fit, it does not divide at all: it saturates, and the sign
// of the saturation is the sign of the result. The engine leans on this - it is
// how a divide by a near-zero denominator stays finite instead of trapping.
auto tFixedDivSaturates = test("Fixed/divSaturatesRatherThanOverflowing") = []
{
    check(FixedDiv(one, 1) == DOOM_MAXINT);
    check(FixedDiv(-one, 1) == DOOM_MININT);
    check(FixedDiv(one, -1) == DOOM_MININT);
};

// Dividing and multiplying back is lossy - there are only 16 bits of fraction to
// round into - but it must be lossy by a hair, not by a unit of world space. A
// fixed_t unit is 1/65536 of a map unit; the player is 32 map units wide.
auto tFixedRoundTripLosesAlmostNothing = test("Fixed/mulAndDivRoundTrip") = []
{
    for (auto value = one; value < 100 * one; value += 7 * one)
    {
        for (auto divisor = one; divisor < 9 * one; divisor += one)
        {
            auto back = FixedMul(FixedDiv(value, divisor), divisor);
            auto lost = back > value ? back - value : value - back;

            check(lost <= 8);
        }
    }
};

// ---------------------------------------------------------------------------
// The random table. DOOM is not random: it walks 256 fixed bytes with an index.
// ---------------------------------------------------------------------------

auto tRandomIsInRange = test("Random/inRange") = []
{
    M_ClearRandom();

    for (auto i = 0; i < 1000; ++i)
    {
        auto value = P_Random();
        check(value >= 0 && value <= 255);
    }
};

// The sequence itself, pinned. These are the first eight bytes of rndtable, and
// they are not an implementation detail: every demo ever recorded, and every
// save game, depends on this exact walk.
auto tRandomSequenceIsFixed = test("Random/sequenceIsTheTable") = []
{
    M_ClearRandom();

    check(P_Random() == 8);
    check(P_Random() == 109);
    check(P_Random() == 220);
    check(P_Random() == 222);
    check(P_Random() == 241);
    check(P_Random() == 149);
    check(P_Random() == 107);
    check(P_Random() == 75);
};

// P_Random starts at index 1, not 0: it increments before it reads.
auto tRandomAdvancesBeforeReading = test("Random/advancesBeforeReading") = []
{
    M_ClearRandom();
    check(prndindex == 0);

    P_Random();
    check(prndindex == 1);
    check(rndtable[1] == 8);
};

// The index wraps at 256 and the sequence repeats exactly.
auto tRandomWraps = test("Random/wrapsAfter256") = []
{
    M_ClearRandom();

    auto first = int {};
    for (auto i = 0; i < 256; ++i)
        if (i == 0)
            first = P_Random();
        else
            P_Random();

    check(prndindex == 0);
    check(P_Random() == first);
};

// The two sequences are independent, which is the whole point of having two: the
// menu and the sounds may vary freely without moving the world.
auto tRandomSequencesAreIndependent = test("Random/playAndMenuAreIndependent") = []
{
    M_ClearRandom();

    P_Random();
    P_Random();
    P_Random();

    check(prndindex == 3);
    check(rndindex == 0);

    M_Random();

    check(prndindex == 3);
    check(rndindex == 1);
};

auto tClearRandomResetsBoth = test("Random/clearResetsBoth") = []
{
    P_Random();
    M_Random();
    M_ClearRandom();

    check(prndindex == 0);
    check(rndindex == 0);
};

// ---------------------------------------------------------------------------
// The trig tables. DOOM does no trigonometry at runtime; it looks it up.
// ---------------------------------------------------------------------------

auto tFineCosineIsFineSineShifted = test("Tables/cosineIsSineQuarterTurnOn") = []
{
    // Not an accident of layout but the definition: finecosine points a quarter
    // of a circle into finesine, which is why finesine is 5/4 of a table long.
    check(finecosine == finesine + FINEANGLES / 4);
};

// The table is sampled at the CENTRE of each fine-angle bucket, not at its edge:
// entry i is sin((i + 0.5) * 2pi / 8192). So none of the cardinals land on the
// exact value, and sin(0) is 25 rather than 0.
//
// It looks like an off-by-one and it is not. Every demo ever recorded, and every
// monster's aim, was computed through these exact numbers - "correcting" the
// table to hit 0 and FRACUNIT squarely would shift the whole game a fraction of
// a degree and desync all of it. Pinned here so the correction is caught by a
// test that explains itself rather than by a mystery desync.
auto tFineSineIsSampledAtBucketCentres =
    test("Tables/sineIsSampledAtBucketCentres") = []
{
    check(finesine[0] == 25); // sin(~0.02 deg)
    check(finesine[FINEANGLES / 4] == 65535); // sin(~90 deg), not 65536
    check(finesine[FINEANGLES / 2] == -25); // sin(~180 deg)
    check(finesine[3 * FINEANGLES / 4] == -65535); // sin(~270 deg)

    check(finecosine[0] == 65535); // cos(~0 deg)
    check(finecosine[FINEANGLES / 4] == -25); // cos(~90 deg)

    // Near enough to the real thing to be the real thing, one part in 65536.
    check(finesine[FINEANGLES / 4] > FRACUNIT - 4);
};

auto tFineSineStaysInUnitRange = test("Tables/sineStaysInUnitRange") = []
{
    for (auto i = 0; i < FINEANGLES; ++i)
        check(finesine[i] >= -FRACUNIT && finesine[i] <= FRACUNIT);
};

auto tTanToAngleIsMonotonic = test("Tables/tanToAngleIsMonotonic") = []
{
    // The table turns a slope into an angle, so it must never go backwards - a
    // steeper slope is always a wider angle. Doom::pointToAngle indexes straight
    // into it and would aim the player wrongly if it did.
    for (auto i = 1; i <= SLOPERANGE; ++i)
        check(tantoangle[i] >= tantoangle[i - 1]);

    check(tantoangle[0] == 0);
};

// Doom::slopeDiv gives up rather than divide by anything under 512 - it answers
// SLOPERANGE, the steepest slope it can name, whatever the numerator. So
// Doom::slopeDiv(0, 1) is 2048 and not 0, which reads as a bug and is not one: the
// result indexes tantoangle, the guard is what keeps the index in the table, and
// Doom::pointToAngle only ever calls it with a denominator it has already made large.
auto tSlopeDivGivesUpOnSmallDenominators =
    test("Tables/slopeDivGivesUpOnSmallDenominators") = []
{
    check(Doom::slopeDiv(0, 1) == SLOPERANGE);
    check(Doom::slopeDiv(1, 1) == SLOPERANGE);
    check(Doom::slopeDiv(0, 511) == SLOPERANGE);

    // At 512 it starts actually dividing.
    check(Doom::slopeDiv(0, 512) == 0);
};

auto tSlopeDivStaysInTable = test("Tables/slopeDivStaysInTable") = []
{
    // Whatever it is handed, the answer must index tantoangle, whose last entry
    // is SLOPERANGE. Anything past that reads off the end of the table.
    check(Doom::slopeDiv(1000000, 512) == SLOPERANGE);
    check(Doom::slopeDiv(1, 1000000) == 0);
    check(Doom::slopeDiv(1000000, 1000000) <= SLOPERANGE);
    check(Doom::slopeDiv(0, 1000000) <= SLOPERANGE);
};

// ---------------------------------------------------------------------------
// Geometry. Cheap approximations the simulation makes everywhere.
// ---------------------------------------------------------------------------

// DOOM never takes a square root to find a distance. It uses an octagonal
// approximation that overestimates diagonals by up to ~12%, and every sight
// check, every "am I close enough to attack", every sound falloff is built on
// it. It is not a bug to be fixed: the monsters' behaviour is tuned to it.
auto tAproxDistanceIsOctagonal = test("Geometry/aproxDistanceIsOctagonal") = []
{
    check(P_AproxDistance(3 * one, 0) == 3 * one);
    check(P_AproxDistance(0, 3 * one) == 3 * one);
    check(P_AproxDistance(-3 * one, 0) == 3 * one);

    // The diagonal: dx + dy - min/2, not the hypotenuse. A true 3-4-5 triangle
    // would give 5; this gives 3 + 4 - 3/2 = 5.5.
    check(P_AproxDistance(4 * one, 3 * one) == 4 * one + 3 * one - (3 * one) / 2);

    // Which is to say it is deliberately wrong, and must stay wrong.
    check(P_AproxDistance(4 * one, 3 * one) != 5 * one);
};

// Doom::pointToAngle inherits the trig table's half-bucket offset, so it lands one
// unit BELOW the exact cardinal - due north is 0x3fffffff, not ANG90. (Due south
// is exact, because it is reached by negating rather than by a lookup.) An angle
// is 1/2^32 of a circle, so one unit is nothing to look at and everything to
// replay: it is what the recorded demos were aimed with.
auto tPointToAngleCardinals = test("Geometry/pointToAngleCardinals") = []
{
    check(Doom::pointToAngle2(0, 0, one, 0) == 0); // east, exact
    check(Doom::pointToAngle2(0, 0, 0, one) == ANG90 - 1); // north
    check(Doom::pointToAngle2(0, 0, -one, 0) == ANG180 - 1); // west
    check(Doom::pointToAngle2(0, 0, 0, -one) == ANG270); // south, exact
};

auto tPointToAngleDiagonal = test("Geometry/pointToAngleDiagonal") = []
{
    check(Doom::pointToAngle2(0, 0, one, one) == ANG45 - 1);
    check(Doom::pointToAngle2(0, 0, -one, one) == ANG90 + ANG45);
};

// Whatever the exact values, the four quadrants must at least be the four
// quadrants - this is the property a rewrite would actually break.
auto tPointToAngleQuadrants = test("Geometry/pointToAngleQuadrants") = []
{
    check(Doom::pointToAngle2(0, 0, one, one) < ANG90); // NE
    check(Doom::pointToAngle2(0, 0, -one, one) > ANG90); // NW
    check(Doom::pointToAngle2(0, 0, -one, one) < ANG180);
    check(Doom::pointToAngle2(0, 0, -one, -one) > ANG180); // SW
    check(Doom::pointToAngle2(0, 0, -one, -one) < ANG270);
    check(Doom::pointToAngle2(0, 0, one, -one) > ANG270); // SE
};

// The same point is no angle at all, rather than an out-of-range index.
auto tPointToAngleDegenerate = test("Geometry/pointToAngleAtSamePoint") = []
{ check(Doom::pointToAngle2(100, 100, 100, 100) == 0); };

// ---------------------------------------------------------------------------
// The tables, whole.
//
// Everything above spot-checks: finesine[0], tantoangle[0], the first eight
// P_Randoms. That is the right shape for a property, and the wrong shape for a
// transcription. Step 3 of REFACTOR.md turns tables.c (2,130 lines) and info.c
// (4,663 lines) into constexpr arrays, and a spot-check would happily pass over
// a single mistyped digit in the middle of 16,000 numbers.
//
// So: every entry, checksummed. If one of these fails, a table was mistranscribed
// and the number below says which table. Nothing else in the suite would tell you
// that - a demo would just desync somewhere and blame the physics.
// ---------------------------------------------------------------------------

auto tFineSineTableIsIntact = test("Tables/fineSineIsIntact") = []
{
    auto sum = Checksum {};

    for (auto i = 0; i < 5 * FINEANGLES / 4; ++i)
        sum << finesine[i];

    checkTable("finesine", sum, 0xd68e94130bb61a68ULL);
};

auto tFineTangentTableIsIntact = test("Tables/fineTangentIsIntact") = []
{
    auto sum = Checksum {};

    for (auto i = 0; i < FINEANGLES / 2; ++i)
        sum << finetangent[i];

    checkTable("finetangent", sum, 0xa0ba8deb9438b0dbULL);
};

auto tTanToAngleTableIsIntact = test("Tables/tanToAngleIsIntact") = []
{
    auto sum = Checksum {};

    for (auto i = 0; i <= SLOPERANGE; ++i)
        sum << tantoangle[i];

    checkTable("tantoangle", sum, 0x373392e3c4a34270ULL);
};

auto tRandomTableIsIntact = test("Random/tableIsIntact") = []
{
    auto sum = Checksum {};

    for (auto i = 0; i < 256; ++i)
        sum << rndtable[i];

    checkTable("rndtable", sum, 0xff36db03f01bc0f7ULL);
};

// The action pointer is deliberately left out: it is a function address, and it
// differs between builds of the same engine. Everything the simulation actually
// reads out of a state goes in.
auto tStateTableIsIntact = test("Info/stateTableIsIntact") = []
{
    auto sum = Checksum {};

    for (auto i = 0; i < Doom::NUMSTATES; ++i)
    {
        const auto& state = states[i];

        sum << state.sprite << state.frame << state.tics << state.nextstate
            << state.misc1 << state.misc2;
    }

    checkTable("states[]", sum, 0x9a0e177f5784c873ULL);
};

auto tMobjInfoTableIsIntact = test("Info/mobjInfoTableIsIntact") = []
{
    auto sum = Checksum {};

    for (auto i = 0; i < Doom::NUMMOBJTYPES; ++i)
    {
        const auto& info = mobjinfo[i];

        sum << info.doomednum << info.spawnstate << info.spawnhealth << info.seestate
            << info.seesound << info.reactiontime << info.attacksound
            << info.painstate << info.painchance << info.painsound << info.meleestate
            << info.missilestate << info.deathstate << info.xdeathstate
            << info.deathsound << info.speed << info.radius << info.height
            << info.mass << info.damage << info.activesound << info.flags
            << info.raisestate;
    }

    checkTable("mobjinfo[]", sum, 0x55b7217727afe6b6ULL);
};
