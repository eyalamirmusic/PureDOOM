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
// places to mean "24.0", and operator*(int, Fixed) gives exactly that. FRACBITS
// stays an integer because it is a shift count.
using fixed_t = Doom::Fixed;

#define FRACBITS 16
#define FRACUNIT (Doom::Fixed {1 << FRACBITS})

// Vanilla's three arithmetic entry points. They are now thin spellings of the
// operators - kept because 150-odd call sites read better as FixedMul(a, b) than
// as (a * b) in the middle of a projection, and because FixedDiv's saturating
// behaviour is a documented engine dependency distinct from FixedDiv2's.
inline fixed_t FixedMul(fixed_t a, fixed_t b) { return a * b; }
inline fixed_t FixedDiv(fixed_t a, fixed_t b) { return Doom::fixedDiv(a, b); }
inline fixed_t FixedDiv2(fixed_t a, fixed_t b) { return Doom::fixedDivUnchecked(a, b); }
