#pragma once

#include "Common.h"

namespace PureDoom
{
struct ScreenVertex
{
    float corner[2];
};

inline constexpr ScreenVertex unitQuad[] = {
    {{0.0f, 0.0f}},
    {{1.0f, 0.0f}},
    {{1.0f, 1.0f}},
    {{0.0f, 0.0f}},
    {{1.0f, 1.0f}},
    {{0.0f, 1.0f}},
};

// Draws the DOOM frame natively: the engine's palette-indexed framebuffer is
// sampled as an R8 texture and looked up in a 256x1 palette texture, so the
// only CPU pixel work left is the engine's own software rasterizer. The unit
// quad maps onto dstRect (view points) — the letterboxed 4:3 area.
struct ScreenShader final : GPU::ShaderProgram
{
    ScreenShader() { compile(); }

    void define() override
    {
        auto corner = vertexInput(&ScreenVertex::corner);

        auto x = dstOrigin.x() + corner.x() * dstSize.x();
        auto y = dstOrigin.y() + corner.y() * dstSize.y();
        auto ndcX = x / viewSize.x() * 2.0f - 1.0f;
        auto ndcY = 1.0f - y / viewSize.y() * 2.0f;
        setPosition(float4(ndcX, ndcY, 0.0f, 1.0f));

        auto v = uvY.x() + corner.y() * (uvY.y() - uvY.x());
        auto uv = varying(float2(corner.x(), v));
        auto index = sample(screenIndices, uv).x();
        auto paletteU = (index * 255.0f + 0.5f) / 256.0f;
        auto color = sample(palette, float2(paletteU, 0.5f));
        setFragment(float4(color.xyz(), 1.0f));
    }

    GPU::Uniform<GPU::Float2> viewSize;
    GPU::Uniform<GPU::Float2> dstOrigin;
    GPU::Uniform<GPU::Float2> dstSize;

    // Vertical source window into the frame (start, end in 0..1): the full
    // frame is {0, 1}, the status-bar strip {168/200, 1}.
    GPU::Uniform<GPU::Float2> uvY;
    GPU::Uniform<GPU::Texture2D> screenIndices;
    GPU::Uniform<GPU::Texture2D> palette;

    EACP_SHADER(viewSize, dstOrigin, dstSize, uvY, screenIndices, palette)
};
} // namespace PureDoom
