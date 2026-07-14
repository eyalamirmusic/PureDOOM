#pragma once

#include "Angle.h"
#include "Fixed.h"

#include <array>

namespace Doom
{
// DOOM does no trigonometry at runtime. It looks it up.
constexpr auto fineAngles = 8192;
constexpr auto fineMask = fineAngles - 1;

constexpr auto slopeRange = 2048;
constexpr auto slopeBits = 11;
constexpr auto slopeToFixedShift = fracBits - slopeBits;

// A quarter turn of sine is also cosine, so one table serves both: the cosine of
// bucket i is the sine of bucket i + a quarter circle. That is why the sine table
// is five quarters long rather than one whole.
constexpr auto fineSineCount = 5 * fineAngles / 4;
constexpr auto fineTangentCount = fineAngles / 2;

// The tables are sampled at the CENTRE of each bucket, not at its edge: entry i
// is sin((i + 0.5) * 2pi / 8192). So no cardinal lands on its exact value -
// sin(0) is 25 rather than 0, and sin(90 degrees) is 65535 rather than 65536.
//
// It reads as an off-by-one and it is not. Every demo ever recorded, and every
// monster's aim, was computed through these exact numbers; squaring the table up
// to hit 0 and fracUnit cleanly would shift the whole game a fraction of a degree
// and desync all of it. Tests/Sim/PrimitiveTests.cpp pins it, and checksums the
// tables whole.
// The storage is the raw 32-bit words, and the type lives in the accessors below
// rather than in the table. Wrapping sixteen thousand literals in `Fixed {...}`
// would be a sixteen-thousand-line diff on data whose whole point is that it has
// not changed - and it is the data, not its spelling, that the demos replay
// through. The tables are `const` and defined once, not `constexpr` in a header:
// they are read at runtime, and a header copy in each of sixty translation units
// buys nothing and costs compile time.
extern const std::array<std::int32_t, fineSineCount> fineSineTable;
extern const std::array<std::int32_t, fineTangentCount> fineTangentTable;

// Maps a slope to the angle that has it. The +1 entry is there so that the x == y
// case needs no extra check.
extern const std::array<std::uint32_t, slopeRange + 1> tanToAngleTable;

inline Fixed fineSine(int fineIndex)
{
    return Fixed {fineSineTable[fineIndex]};
}

inline Fixed fineCosine(int fineIndex)
{
    return Fixed {fineSineTable[fineIndex + fineAngles / 4]};
}

inline Fixed fineTangent(int fineIndex)
{
    return Fixed {fineTangentTable[fineIndex]};
}

inline Fixed sine(Angle angle)
{
    return fineSine(angle.fineIndex());
}
inline Fixed cosine(Angle angle)
{
    return fineCosine(angle.fineIndex());
}

inline Angle tanToAngle(int slope)
{
    return Angle {tanToAngleTable[slope]};
}

// Turns a slope into an index into tanToAngleTable.
//
// It gives up on any denominator under 512 and answers slopeRange - the steepest
// slope it can name - whatever the numerator, so slopeDiv(0, 1) is 2048 and not
// 0. That reads as a bug and is not one: the result indexes tanToAngleTable, the
// guard is what keeps the index inside it, and R_PointToAngle only ever calls it
// with a denominator it has already made large.
int slopeDiv(unsigned num, unsigned den);
} // namespace Doom
