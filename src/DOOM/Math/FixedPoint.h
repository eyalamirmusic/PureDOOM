#pragma once

#include "Fixed.h"

namespace Doom
{
// Defined FROM fracUnit rather than from a fresh 1 << 16, so there is one number
// here and not two.
//
// FRACUNIT is a Fixed of value 1.0 rather than the integer 65536, and that is
// load-bearing: the engine writes `24 * FRACUNIT` in ~380 places to mean "24.0",
// and operator*(int, Fixed) gives exactly that.
//
// FRACBITS used to sit beside it and is retired: it was a thirteenth instance of
// the duplicate-constant category CLAUDE.md records - Fixed::fracBits and
// fracBits already existed, same value, same meaning. It read as harmless because
// a shift count cannot overflow an array, but two sites had already drifted into
// using BOTH spellings inside one expression
// (`(topscreen.raw + fracUnit - 1) >> FRACBITS`), which is the ANG*/ang* problem
// again: nothing told a reader they were the same constant. Use fracBits.
//
// `fixed_t` used to be aliased here onto Fixed and is gone: the engine spells the
// strong type by its own name everywhere now.
constexpr Fixed FRACUNIT {fracUnit};

// Two of vanilla's three arithmetic entry points. They are now thin spellings of
// the operators - kept because 150-odd call sites read better as FixedMul(a, b)
// than as (a * b) in the middle of a projection. FixedDiv2 was the third and is
// gone: nothing called it, and the unsaturated division it named is spelled
// fixedDivUnchecked (Math/Fixed.h), which is what FixedDiv falls through to and
// what Tests/Sim/PrimitiveTests.cpp pins the double-precision behaviour on.
inline Fixed FixedMul(Fixed a, Fixed b)
{
    return a * b;
}
inline Fixed FixedDiv(Fixed a, Fixed b)
{
    return fixedDiv(a, b);
}
} // namespace Doom
