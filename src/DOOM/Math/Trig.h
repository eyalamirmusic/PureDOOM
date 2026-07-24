#pragma once

#include "Angle.h"
#include "Fixed.h"

#include "../Containers.h"

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
// through. The tables are `const` and defined once in Math/Trig.cpp, not
// `constexpr` in a header: they are read at runtime, and a header copy in each of
// sixty translation units buys nothing and costs compile time. `const` at
// namespace scope has internal linkage, so the three tables are file-local to
// Trig.cpp (which is the only reader) and need no declaration here.

// The typed views onto those tables - a `const T*` reinterpreting the raw words as
// Fixed / Angle. Free functions (defined in Trig.cpp, where the tables live)
// because the tables are file-local there; they were four `extern const T*` globals
// the whole engine indexed as finesine[i]. finecosine() is finesine() advanced a
// quarter circle, which is why finesine's table is 5/4 of a circle long.
const Fixed* finesine();
const Fixed* finecosine();
const Fixed* finetangent();
const Angle* tantoangle();

// The typed lookups the engine reads a single entry through. Thin spellings over
// the views above, so they need no direct sight of the tables.
inline Fixed fineSine(int fineIndex)
{
    return finesine()[fineIndex];
}

inline Fixed fineCosine(int fineIndex)
{
    return finecosine()[fineIndex];
}

inline Fixed fineTangent(int fineIndex)
{
    return finetangent()[fineIndex];
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
    return tantoangle()[slope];
}

// Turns a slope into an index into tanToAngleTable.
//
// It gives up on any denominator under 512 and answers slopeRange - the steepest
// slope it can name - whatever the numerator, so slopeDiv(0, 1) is 2048 and not
// 0. That reads as a bug and is not one: the result indexes tanToAngleTable, the
// guard is what keeps the index inside it, and pointToAngle only ever calls it
// with a denominator it has already made large.
int slopeDiv(unsigned num, unsigned den);
} // namespace Doom
