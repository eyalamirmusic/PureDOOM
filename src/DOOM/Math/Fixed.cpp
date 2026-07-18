#include "Fixed.h"
#include "../Host/Platform.h"
#include "FixedPoint.h"


#include "../Host/System.h"
#include "../Game/GameDefs.h"
namespace Doom
{
Fixed fixedDivUnchecked(Fixed a, Fixed b)
{
    auto quotient = ((double) a.raw) / ((double) b.raw) * (double) fracUnit;

    if (quotient >= 2147483648.0 || quotient < -2147483648.0)
        fatalError("Error: FixedDiv: divide by zero");

    return Fixed {(std::int32_t) quotient};
}

Fixed fixedDiv(Fixed a, Fixed b)
{
    // The guard is not "is b zero" but "will the quotient fit": shifting the
    // numerator down by 14 asks whether it is more than four times the
    // denominator's magnitude, which is exactly when 16.16 overflows.
    if ((abs(a).raw >> 14) >= abs(b).raw)
        return (a.raw ^ b.raw) < 0 ? Fixed {DOOM_MININT} : Fixed {DOOM_MAXINT};

    return fixedDivUnchecked(a, b);
}
} // namespace Doom

// ---------------------------------------------------------------------------
// Global-scope data that was m_fixed.cpp. It stays at :: scope because these are the
// vanilla names other translation units (and the eacp port) still link against.
// ---------------------------------------------------------------------------
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
