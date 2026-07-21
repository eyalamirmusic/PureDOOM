#pragma once

#include "DoomShader.h"

namespace PureDoom
{
// The screen melt, drawn over the frame the renderer has already produced rather
// than instead of it - which is what keeps the level it is revealing at the
// window's resolution.
//
// The melt only ever reads the outgoing screen, so that is the only one that has
// to be a texture: what shows above it is the framebuffer left alone, the new
// level. That is why this needs no offscreen render target. The outgoing screen
// stays 320x200, and should - it is title or intermission artwork, on its way
// out.
struct WipeShader final : ScreenQuadShader
{
    WipeShader() { compile(); }

    void define() override
    {
        auto corner = quadCorner();
        setQuadPosition(corner);

        auto uv = varying(float2(corner.x(), corner.y()));

        // One offset per two-pixel column, sampled nearest, so the lookup picks
        // the column out on its own with no rounding to do by hand.
        auto slid = sample(offsets, float2(uv.x(), 0.5f)).x() * 255.0f;
        auto sourceRow = uv.y() * (float) Engine::screenHeight - slid;

        // Above where the column has slid to, the outgoing screen is gone, and
        // throwing the pixel away is what leaves the new frame standing.
        setDiscardBelow(sourceRow, 0.0f);

        auto uvStart = float2(uv.x(), sourceRow / (float) Engine::screenHeight);
        setPaletteFragment(indexOf(sample(start, uvStart)));
    }

    GPU::Uniform<GPU::Texture2D> start;
    GPU::Uniform<GPU::Texture2D> offsets;

    EACP_SHADER(viewSize, dstOrigin, dstSize, start, offsets, palette)
};
} // namespace PureDoom
