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

    // Addition, subtraction and negation wrap at 32 bits, and the engine leans
    // on it: storeWallRange legitimately overflows a texture offset on E1M1's
    // first attract-mode frame, and the goldens are recorded against the
    // wrapped value. Signed overflow being UB, the wrap is computed in
    // unsigned - where it is defined - and converted back: the same bits, now
    // guaranteed. Tests/Sim/MathTests.cpp pins the wrapped answers.
    static constexpr std::uint32_t bits(std::int32_t value)
    {
        return static_cast<std::uint32_t>(value);
    }

    static constexpr std::int32_t wrapped(std::uint32_t value)
    {
        return static_cast<std::int32_t>(value);
    }

    static constexpr Fixed fromInt(int value)
    {
        return Fixed {wrapped(bits(value) * fracUnit)};
    }

    constexpr int toInt() const { return raw >> fracBits; }

    constexpr Fixed operator+(Fixed other) const
    {
        return Fixed {wrapped(bits(raw) + bits(other.raw))};
    }

    constexpr Fixed operator-(Fixed other) const
    {
        return Fixed {wrapped(bits(raw) - bits(other.raw))};
    }

    constexpr Fixed operator-() const { return Fixed {wrapped(0u - bits(raw))}; }

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

    // Raw shifts. DOOM halves and doubles fixed values with these all over the
    // playsim and the renderer; they scale the value, they do not convert it.
    // (Converting to and from whole units is toInt() / fromInt().)
    constexpr Fixed operator>>(int shift) const { return Fixed {raw >> shift}; }
    constexpr Fixed operator<<(int shift) const { return Fixed {raw << shift}; }
    constexpr Fixed& operator>>=(int shift) { return *this = *this >> shift; }
    constexpr Fixed& operator<<=(int shift) { return *this = *this << shift; }

    // Sign tests. Spelled out rather than allowing comparison against a bare int,
    // which would silently read the literal as a raw value: `x > 1` would mean
    // 1/65536 of a unit, not one unit.
    constexpr bool isNegative() const { return raw < 0; }

    // Explicit, so `if (momx)` and `while (xmove || ymove)` read as they always
    // did - a fixed value is "true" when it is non-zero - without letting a Fixed
    // silently become an int in arithmetic.
    constexpr explicit operator bool() const { return raw != 0; }
    constexpr bool isPositive() const { return raw > 0; }
    constexpr bool isZero() const { return raw == 0; }

    Fixed& operator*=(Fixed other) { return *this = *this * other; }
    Fixed& operator/=(Fixed other) { return *this = *this / other; }
};

constexpr auto fracBits = Fixed::fracBits;
constexpr auto fracUnit = Fixed::fracUnit;

constexpr Fixed operator*(Fixed value, int scale)
{
    return Fixed {Fixed::wrapped(Fixed::bits(value.raw) * Fixed::bits(scale))};
}

constexpr Fixed operator*(int scale, Fixed value)
{
    return value * scale;
}

// Integer division of a fixed value - scaling it down by a whole number, as in
// FRACUNIT / 4 or MAXMOVE / 2. Distinct from Fixed / Fixed, which is the
// saturating fixed-point divide.
constexpr Fixed operator/(Fixed value, int divisor)
{
    return Fixed {value.raw / divisor};
}

constexpr Fixed abs(Fixed value)
{
    return value.raw < 0 ? -value : value;
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
