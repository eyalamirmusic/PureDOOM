#pragma once

#include "DoomShader.h"

namespace PureDoom
{
// The screen melt when the frame being slid away is one the engine drew: the
// title, an intermission or the finale, on its way out. It is 320x200 artwork and
// loses nothing by staying 320x200 here.
//
// A melt out of a *level* is CaptureShader's, the outgoing frame there being one
// this renderer composited and worth keeping at the window's resolution. The two
// share the melt's rule and nothing else - this one resolves indices through the
// palette, that one slides colour that was resolved when it was captured.
//
// Drawn over the frame the renderer has already produced rather than instead of
// it: the melt only ever reads the outgoing screen, so what shows above it is the
// framebuffer left alone, the new level at its own resolution.
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
