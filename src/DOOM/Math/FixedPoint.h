#pragma once

#include "Fixed.h"

// The vanilla spelling of the fixed-point type and its unit.
//
// fixed_t IS Doom::Fixed now - the strong type - rather than a bare int. That is
// what stops a raw integer being used where a 16.16 fixed-point value is meant,
// which is a whole class of DOOM bug the compiler could not see before.
//
// FRACUNIT is a Fixed of value 1.0 rather than the integer 65536, and that is
// load-bearing for the migration: the engine writes `24 * FRACUNIT` in ~380
// places to mean "24.0", and operator*(int, Fixed) gives exactly that.
using fixed_t = Doom::Fixed;

// Deliberately at :: scope, like fixed_t and FixedMul above: Tests/Sim/
// PrimitiveTests.cpp and examples/EACP/EngineAccess.cpp read FRACUNIT unqualified
// and neither has a `using namespace Doom`. Defined FROM Doom::fracUnit rather
// than from a fresh 1 << 16, so there is one number here and not two.
//
// FRACBITS used to sit beside it and is retired: it was a thirteenth instance of
// the duplicate-constant category REFACTOR.md item 6 tabulates - Fixed::fracBits
// and Doom::fracBits already existed, same value, same meaning. It read as
// harmless because a shift count cannot overflow an array, but two sites had
// already drifted into using BOTH spellings inside one expression
// (`(topscreen.raw + fracUnit - 1) >> FRACBITS`), which is the ANG*/ang* problem
// again: nothing told a reader they were the same constant. Use Doom::fracBits.
constexpr Doom::Fixed FRACUNIT {Doom::fracUnit};

// Vanilla's three arithmetic entry points. They are now thin spellings of the
// operators - kept because 150-odd call sites read better as FixedMul(a, b) than
// as (a * b) in the middle of a projection, and because FixedDiv's saturating
// behaviour is a documented engine dependency distinct from FixedDiv2's.
inline fixed_t FixedMul(fixed_t a, fixed_t b)
{
    return a * b;
}
inline fixed_t FixedDiv(fixed_t a, fixed_t b)
{
    return Doom::fixedDiv(a, b);
}
inline fixed_t FixedDiv2(fixed_t a, fixed_t b)
{
    return Doom::fixedDivUnchecked(a, b);
}
