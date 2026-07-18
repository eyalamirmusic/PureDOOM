// The vanilla fixed-point API, now a shim over Doom::Fixed (Math/Fixed.h).
//
// It stays because most of the engine is still vanilla-shaped and calls it 165
// times. It delegates rather than duplicating, so the new type sits on the
// critical path of every demo the suite replays - which is the only way a new
// implementation of the simulation's arithmetic gets tested at all, and it is why
// the shim is a shim and not a copy.
//
// It goes when the last caller does.

#include "Math/Fixed.h"

#include "Host/Platform.h"
#include "Math/FixedPoint.h"

fixed_t FixedMul(fixed_t a, fixed_t b)
{
    return (Doom::Fixed {a} * Doom::Fixed {b}).raw;
}

fixed_t FixedDiv(fixed_t a, fixed_t b)
{
    return Doom::fixedDiv(Doom::Fixed {a}, Doom::Fixed {b}).raw;
}

fixed_t FixedDiv2(fixed_t a, fixed_t b)
{
    return Doom::fixedDivUnchecked(Doom::Fixed {a}, Doom::Fixed {b}).raw;
}
