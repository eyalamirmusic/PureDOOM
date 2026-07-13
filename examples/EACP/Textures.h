#pragma once

#include "Common.h"

namespace PureDoom
{
// The engine's own framebuffer, one palette index per pixel.
inline GPU::Texture makeIndexTexture()
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = doomWidth;
    descriptor.height = doomHeight;
    descriptor.format = GPU::TextureFormat::R8Unorm;
    descriptor.filter = GPU::TextureFilter::Nearest;

    return GPU::Device::shared().makeTexture(descriptor, nullptr);
}

inline GPU::Texture makePaletteTexture()
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = 256;
    descriptor.height = 1;
    descriptor.filter = GPU::TextureFilter::Nearest;

    return GPU::Device::shared().makeTexture(descriptor, nullptr);
}

// One row per light level. Clamped, so the shader's row index needs no
// explicit clamp: a surface too bright or too far simply lands on the first or
// last row.
inline GPU::Texture makeColormapTexture()
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = 256;
    descriptor.height = EACP_DOOM_COLORMAP_ROWS;
    descriptor.format = GPU::TextureFormat::R8Unorm;
    descriptor.filter = GPU::TextureFilter::Nearest;

    return GPU::Device::shared().makeTexture(descriptor, nullptr);
}

// Wall textures and flats tile across a surface, so they repeat; they carry
// palette indices, which must never be blended, so they sample nearest.
inline GPU::Texture
    makeWorldTexture(int width, int height, const std::uint8_t* pixels)
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = width;
    descriptor.height = height;
    descriptor.format = GPU::TextureFormat::R8Unorm;
    descriptor.filter = GPU::TextureFilter::Nearest;
    descriptor.addressMode = GPU::TextureAddressMode::Repeat;

    return GPU::Device::shared().makeTexture(descriptor, pixels);
}
} // namespace PureDoom
