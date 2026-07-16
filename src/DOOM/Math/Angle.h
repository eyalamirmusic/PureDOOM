#pragma once

#include <compare>
#include <cstdint>

namespace Doom
{
// Binary Angle Measurement: a whole circle in 2^32 units, so it wraps by itself
// and never needs a modulo. One unit is 1/4294967296 of a turn, which is far
// below anything a player could see and exactly what a demo replays.
struct Angle
{
    std::uint32_t raw = 0;

    constexpr Angle() = default;
    explicit constexpr Angle(std::uint32_t rawToUse)
        : raw(rawToUse)
    {
    }

    static constexpr Angle fromDegrees(double degrees)
    {
        return Angle {(std::uint32_t) (degrees / 360.0 * 4294967296.0)};
    }

    constexpr double toDegrees() const { return raw / 4294967296.0 * 360.0; }

    constexpr Angle operator+(Angle other) const { return Angle {raw + other.raw}; }
    constexpr Angle operator-(Angle other) const { return Angle {raw - other.raw}; }
    constexpr Angle operator-() const { return Angle {0u - raw}; }

    constexpr Angle& operator+=(Angle other) { return *this = *this + other; }
    constexpr Angle& operator-=(Angle other) { return *this = *this - other; }

    constexpr auto operator<=>(const Angle&) const = default;
    constexpr bool operator==(const Angle&) const = default;

    // Which of the 8192 fine-angle buckets this lands in - the index the trig
    // tables are read with.
    constexpr int fineIndex() const
    {
        return static_cast<int>(raw >> angleToFineShift);
    }

    static constexpr auto angleToFineShift = 19;
};

constexpr auto ang45 = Angle {0x20000000};
constexpr auto ang90 = Angle {0x40000000};
constexpr auto ang180 = Angle {0x80000000};
constexpr auto ang270 = Angle {0xc0000000};
} // namespace Doom
