#pragma once

#include "Common.h"

namespace PureDoom
{
// How a texture is sampled is not set here: it belongs to the shader that
// samples it, as a GPU::TextureSampling on its texture uniform, fixed when the
// shader compiles. Every texture below wants what that defaults to - nearest,
// clamped - so only the two shaders sampling makeWorldTexture say anything.

// The engine's own framebuffer, one palette index per pixel.
inline GPU::Texture makeIndexTexture()
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = Engine::screenWidth;
    descriptor.height = Engine::screenHeight;
    descriptor.format = GPU::TextureFormat::R8Unorm;

    return GPU::Device::shared().makeTexture(descriptor, nullptr);
}

inline GPU::Texture makePaletteTexture()
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = 256;
    descriptor.height = 1;

    return GPU::Device::shared().makeTexture(descriptor, nullptr);
}

// One row per light level. It is fetched by texel rather than sampled, so no
// address mode bounds it and DoomShader::remap does the clamping.
inline GPU::Texture makeColormapTexture()
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = 256;
    descriptor.height = Engine::colormapRows;
    descriptor.format = GPU::TextureFormat::R8Unorm;

    return GPU::Device::shared().makeTexture(descriptor, nullptr);
}

// The software-only layers over the view, as palette indices with their coverage
// in alpha - which is what lets the world show through the pixels the engine
// never drew on.
inline GPU::Texture makeOverlayTexture()
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = Engine::screenWidth;
    descriptor.height = Engine::screenHeight;
    descriptor.format = GPU::TextureFormat::RGBA8Unorm;

    return GPU::Device::shared().makeTexture(descriptor, nullptr);
}

// How far each of the melt's two-pixel columns has slid, in rows: one texel per
// column, so sampling it nearest picks the column out on its own.
inline GPU::Texture makeWipeOffsetTexture()
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = Engine::wipeColumns;
    descriptor.height = 1;
    descriptor.format = GPU::TextureFormat::R8Unorm;

    return GPU::Device::shared().makeTexture(descriptor, nullptr);
}

// What the world is rendered into, at the window's own resolution: DOOM's frame
// rather than a picture of it - a palette index per pixel in red, and the fuzz
// mark a spectre leaves in green (DoomShader::setIndexFragment). Its depth
// buffer is the target's own, created with it and gone with it.
//
// RGBA8 for one useful channel and a half, because eacp has no single-channel
// PixelFormat and a render target must name one (see the gap log). The second
// channel is wanted regardless, so what R8 would save is two channels rather
// than three.
inline GPU::Texture makeWorldTarget(int width, int height)
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = width;
    descriptor.height = height;
    descriptor.format = worldTargetFormat;
    descriptor.renderTarget = true;
    descriptor.depth = true;

    return GPU::Device::shared().makeTexture(descriptor, nullptr);
}

// The finished frame, at the window's own resolution and already in colour: what
// the screen is blitted from, and what a melt slides away.
//
// Depth because the shaders that draw into it are the ones prepared for the
// drawable, which has one (GPUView::setDepth) - a pipeline that tests depth
// against a pass with no depth attachment goes on working on Apple silicon and
// stops on D3D12, which is the trap the gap log's entry 7 records.
inline GPU::Texture makeScreenCapture(int width, int height)
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = width;
    descriptor.height = height;
    descriptor.format = captureTargetFormat;
    descriptor.renderTarget = true;
    descriptor.depth = true;

    return GPU::Device::shared().makeTexture(descriptor, nullptr);
}

// Wall textures, flats and sprites tile across a surface, so WorldShader and
// HudShader declare them repeating; they carry palette indices, which must never
// be blended, so those shaders sample them nearest. A masked one needs a second
// channel for its coverage, so it goes up as RGBA - index in red, coverage in
// alpha - while the rest stay one byte per pixel.
inline GPU::Texture
    makeWorldTexture(int width, int height, bool masked, const std::uint8_t* pixels)
{
    auto descriptor = GPU::TextureDescriptor {};
    descriptor.width = width;
    descriptor.height = height;
    descriptor.format =
        masked ? GPU::TextureFormat::RGBA8Unorm : GPU::TextureFormat::R8Unorm;

    return GPU::Device::shared().makeTexture(descriptor, pixels);
}
} // namespace PureDoom
