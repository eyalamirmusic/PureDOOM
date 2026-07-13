#pragma once

#include "DoomShader.h"

namespace PureDoom
{
// The engine's own software frame, drawn as it stands: an R8 texture of palette
// indices looked up in the palette, so no pixel is converted on the CPU. This is
// what everything outside a level is - the title, the intermission, the finale -
// and it is where the status bar always comes from.
struct ScreenShader final : ScreenQuadShader
{
    ScreenShader() { compile(); }

    void define() override
    {
        auto corner = quadCorner();
        setQuadPosition(corner);

        auto v = uvY.x() + corner.y() * (uvY.y() - uvY.x());
        auto uv = varying(float2(corner.x(), v));
        setPaletteFragment(indexOf(sample(screenIndices, uv)));
    }

    // The rows of the frame to draw: the whole of it, or just the status bar.
    GPU::Uniform<GPU::Float2> uvY;
    GPU::Uniform<GPU::Texture2D> screenIndices;

    EACP_SHADER(viewSize, dstOrigin, dstSize, uvY, screenIndices, palette)
};
} // namespace PureDoom
