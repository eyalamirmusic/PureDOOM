// Endianness handling for WAD data, which is stored little-endian.
//
// Rewritten from m_swap. The swaps are only reached on a big-endian host (WADs
// are little-endian, so a little-endian host needs none); m_swap.h keeps the
// vanilla SHORT/LONG macros and forwards them here when __BIG_ENDIAN__ is set.

#pragma once

#include <cstdint>

namespace Doom
{

constexpr std::uint16_t swap16(std::uint16_t x)
{
    return (std::uint16_t) ((x >> 8) | (x << 8));
}

constexpr std::uint32_t swap32(std::uint32_t x)
{
    return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000)
         | (x << 24);
}

} // namespace Doom
