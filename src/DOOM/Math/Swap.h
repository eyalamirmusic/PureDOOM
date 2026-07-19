// Endianness handling for WAD data, which is stored little-endian.
//
// Rewritten from m_swap. The swaps are only reached on a big-endian host (WADs
// are little-endian, so a little-endian host needs none).

#pragma once

#include <cstdint>
#include <type_traits>

namespace Doom
{

constexpr std::uint16_t swap16(std::uint16_t x)
{
    return static_cast<std::uint16_t>((x >> 8) | (x << 8));
}

constexpr std::uint32_t swap32(std::uint32_t x)
{
    return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000)
         | (x << 24);
}

// A field whose bytes are little-endian on disk -- a WAD lump, or the PCX header
// a screenshot writes. Identity on a little-endian host, which is exactly what
// vanilla's SHORT() and LONG() macros compiled to there. The swap is its own
// inverse, so one name serves both directions, which is why this is not called
// fromLittleEndian: the same call converts on the way in and on the way out.
//
// The width is deduced rather than named, and that is the point of it. SHORT and
// LONG picked the swap by the *spelling at the call site*, and the two disagreed
// with each other about the result type on a big-endian host: LONG yielded `long`
// -- 8 bytes here -- where the little-endian path yielded the argument's own
// `int`. Deducing preserves the call site's type on both paths, and the
// static_asserts turn a field of some other width into a compile error rather
// than a silent truncation. Where a call site's argument is genuinely wider than
// the on-disk field, it names the width explicitly: littleEndian<unsigned short>.
template <typename T>
constexpr T littleEndian(T x)
{
    static_assert(std::is_integral_v<T>, "an on-disk field is an integer");
    static_assert(sizeof(T) == 2 || sizeof(T) == 4,
                  "an on-disk field is 16- or 32-bit");

#ifdef __BIG_ENDIAN__
    if constexpr (sizeof(T) == 2)
        return static_cast<T>(swap16(static_cast<std::uint16_t>(x)));
    else
        return static_cast<T>(swap32(static_cast<std::uint32_t>(x)));
#else
    return x;
#endif
}

} // namespace Doom
