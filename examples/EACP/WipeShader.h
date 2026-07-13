#pragma once

#include "Common.h"
#include "ScreenShader.h"

namespace PureDoom
{
// The screen melt, over the frame the renderer has already drawn.
//
// DOOM's wipe slides the outgoing screen down a column at a time and lets the
// incoming one show above it. Only the outgoing screen is ever read, so it is
// the only one that has to be a texture: what shows above it is simply the
// framebuffer left alone - the new level, at the window's resolution. That is
// why this needs no offscreen render target, and why the level no longer arrives
// as a 320x200 image that snaps up to full resolution when the melt ends.
//
// The outgoing screen stays 320x200, and should: it is the intermission or the
// title screen, which is 320x200 artwork - and it is on its way out.
struct WipeShader final : GPU::ShaderProgram
{
    WipeShader() { compile(); }

    void define() override
    {
        auto corner = vertexInput(&ScreenVertex::corner);

        auto x = dstOrigin.x() + corner.x() * dstSize.x();
        auto y = dstOrigin.y() + corner.y() * dstSize.y();
        auto ndcX = x / viewSize.x() * 2.0f - 1.0f;
        auto ndcY = 1.0f - y / viewSize.y() * 2.0f;
        setPosition(float4(ndcX, ndcY, 0.0f, 1.0f));

        auto uv = varying(float2(corner.x(), corner.y()));

        // How far this column of the outgoing screen has slid. There is one
        // offset per two-pixel column and it is sampled nearest, so the lookup is
        // the column index: no rounding to do by hand.
        auto slid = sample(offsets, float2(uv.x(), 0.5f)).x() * 255.0f;
        auto row = uv.y() * (float) doomHeight;
        auto sourceRow = row - slid;

        // Above where the column has slid to, the outgoing screen is gone.
        // Throwing the pixel away is what leaves the new frame showing.
        setDiscardBelow(sourceRow, 0.0f);

        auto index =
            sample(start, float2(uv.x(), sourceRow / (float) doomHeight)).x();
        auto color = sample(palette, float2((index * 255.0f + 0.5f) / 256.0f, 0.5f));

        setFragment(float4(color.xyz(), 1.0f));
    }

    GPU::Uniform<GPU::Float2> viewSize;
    GPU::Uniform<GPU::Float2> dstOrigin;
    GPU::Uniform<GPU::Float2> dstSize;

    GPU::Uniform<GPU::Texture2D> start;
    GPU::Uniform<GPU::Texture2D> offsets;
    GPU::Uniform<GPU::Texture2D> palette;

    EACP_SHADER(viewSize, dstOrigin, dstSize, start, offsets, palette)
};
} // namespace PureDoom
