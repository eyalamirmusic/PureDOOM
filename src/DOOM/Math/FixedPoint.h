#pragma once

#include "Fixed.h"

// Deliberately at :: scope: Tests/Sim/PrimitiveTests.cpp and
// examples/EACP/EngineAccess.cpp read FRACUNIT unqualified and neither has a
// `using namespace Doom`. Defined FROM Doom::fracUnit rather than from a fresh
// 1 << 16, so there is one number here and not two.
//
// FRACUNIT is a Doom::Fixed of value 1.0 rather than the integer 65536, and that
// is load-bearing: the engine writes `24 * FRACUNIT` in ~380 places to mean
// "24.0", and operator*(int, Fixed) gives exactly that.
//
// FRACBITS used to sit beside it and is retired: it was a thirteenth instance of
// the duplicate-constant category CLAUDE.md records - Fixed::fracBits
// and Doom::fracBits already existed, same value, same meaning. It read as
// harmless because a shift count cannot overflow an array, but two sites had
// already drifted into using BOTH spellings inside one expression
// (`(topscreen.raw + fracUnit - 1) >> FRACBITS`), which is the ANG*/ang* problem
// again: nothing told a reader they were the same constant. Use Doom::fracBits.
//
// `fixed_t` used to be aliased here onto Doom::Fixed and is gone: the engine
// spells the strong type by its own name everywhere now.
constexpr Doom::Fixed FRACUNIT {Doom::fracUnit};

// Vanilla's three arithmetic entry points. They are now thin spellings of the
// operators - kept because 150-odd call sites read better as FixedMul(a, b) than
// as (a * b) in the middle of a projection, and because FixedDiv's saturating
// behaviour is a documented engine dependency distinct from FixedDiv2's.
inline Doom::Fixed FixedMul(Doom::Fixed a, Doom::Fixed b)
{
    return a * b;
}
inline Doom::Fixed FixedDiv(Doom::Fixed a, Doom::Fixed b)
{
    return Doom::fixedDiv(a, b);
}
inline Doom::Fixed FixedDiv2(Doom::Fixed a, Doom::Fixed b)
{
    return Doom::fixedDivUnchecked(a, b);
}
