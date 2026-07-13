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

// The software-only layers over the view - menu, messages, PAUSE, map marks -
// as palette indices with their coverage in alpha, which is what lets the world
// show through the pixels the engine never drew on.
inline GPU::Texture makeOverlayTexture()
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = EACP_DOOM_SCREEN_WIDTH;
    descriptor.height = EACP_DOOM_SCREEN_HEIGHT;
    descriptor.format = GPU::TextureFormat::RGBA8Unorm;
    descriptor.filter = GPU::TextureFilter::Nearest;

    return GPU::Device::shared().makeTexture(descriptor, nullptr);
}

// How far each of the melt's two-pixel columns has slid, in rows: one texel per
// column, so sampling it nearest picks the column out on its own.
inline GPU::Texture makeWipeOffsetTexture()
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = EACP_DOOM_WIPE_COLUMNS;
    descriptor.height = 1;
    descriptor.format = GPU::TextureFormat::R8Unorm;
    descriptor.filter = GPU::TextureFilter::Nearest;

    return GPU::Device::shared().makeTexture(descriptor, nullptr);
}

// Wall textures, flats and sprites tile across a surface, so they repeat; they
// carry palette indices, which must never be blended, so they sample nearest. A
// masked one needs a second channel for its coverage, so it goes up as RGBA -
// index in red, coverage in alpha - while the rest stay one byte per pixel.
inline GPU::Texture
    makeWorldTexture(int width, int height, bool masked, const std::uint8_t* pixels)
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = width;
    descriptor.height = height;
    descriptor.format =
        masked ? GPU::TextureFormat::RGBA8Unorm : GPU::TextureFormat::R8Unorm;
    descriptor.filter = GPU::TextureFilter::Nearest;
    descriptor.addressMode = GPU::TextureAddressMode::Repeat;

    return GPU::Device::shared().makeTexture(descriptor, pixels);
}
} // namespace PureDoom
