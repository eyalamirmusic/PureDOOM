#pragma once

#include <compare>
#include <cstdint>

namespace Doom
{
// 16.16 fixed point: every position, velocity, height and scale in DOOM is one
// of these. A 32-bit integer holding 16 bits of fraction, so one unit of world
// space is 65536 and the player is 32 units wide.
//
// The whole simulation is deterministic because this arithmetic is. Do not
// "improve" any of it - the demo goldens are recorded against these exact
// operations, and every recorded demo in the world was aimed with them.
struct Fixed
{
    static constexpr auto fracBits = 16;
    static constexpr auto fracUnit = 1 << fracBits;

    std::int32_t raw = 0;

    constexpr Fixed() = default;
    explicit constexpr Fixed(std::int32_t rawToUse)
        : raw(rawToUse)
    {
    }

    static constexpr Fixed fromInt(int value)
    {
        return Fixed {(std::int32_t) (value * fracUnit)};
    }

    constexpr int toInt() const { return raw >> fracBits; }

    constexpr Fixed operator+(Fixed other) const { return Fixed {raw + other.raw}; }
    constexpr Fixed operator-(Fixed other) const { return Fixed {raw - other.raw}; }
    constexpr Fixed operator-() const { return Fixed {-raw}; }

    constexpr Fixed& operator+=(Fixed other) { return *this = *this + other; }
    constexpr Fixed& operator-=(Fixed other) { return *this = *this - other; }

    constexpr auto operator<=>(const Fixed&) const = default;
    constexpr bool operator==(const Fixed&) const = default;

    // The multiply widens to 64 bits before shifting back down, so a product that
    // would overflow 32 bits mid-way still comes out right. Doing the shift in 32
    // bits passes every small case and then teleports objects at speed.
    constexpr Fixed operator*(Fixed other) const
    {
        return Fixed {(std::int32_t) (((std::int64_t) raw * (std::int64_t) other.raw)
                                      >> fracBits)};
    }

    Fixed operator/(Fixed other) const;

    Fixed& operator*=(Fixed other) { return *this = *this * other; }
    Fixed& operator/=(Fixed other) { return *this = *this / other; }
};

constexpr auto fracBits = Fixed::fracBits;
constexpr auto fracUnit = Fixed::fracUnit;

constexpr Fixed operator*(Fixed value, int scale)
{
    return Fixed {(std::int32_t) (value.raw * scale)};
}

constexpr Fixed operator*(int scale, Fixed value)
{
    return value * scale;
}

constexpr Fixed abs(Fixed value)
{
    return value.raw < 0 ? Fixed {-value.raw} : value;
}

// The division that does NOT saturate, and the one place the simulation leaves
// integer arithmetic: it goes through a double.
//
// That is not a bug to be fixed. It is deterministic - the same IEEE-754
// operation every time, which is why demos replay at all - but it is not the
// same rounding a pure-integer division would give, so rewriting this in
// integers desyncs every demo ever recorded. (The engine is built with
// -ffp-contract=off so that nothing may fuse it, either.)
Fixed fixedDivUnchecked(Fixed a, Fixed b);

// Saturates rather than overflowing when the quotient will not fit, and the sign
// of the saturation is the sign of the result. The engine leans on this: it is
// how dividing by a near-zero denominator stays finite instead of trapping.
Fixed fixedDiv(Fixed a, Fixed b);

constexpr Fixed fixedMul(Fixed a, Fixed b)
{
    return a * b;
}

inline Fixed Fixed::operator/(Fixed other) const
{
    return fixedDiv(*this, other);
}
} // namespace Doom
