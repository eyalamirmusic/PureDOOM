#pragma once

#include "DoomShader.h"

namespace PureDoom
{
// The composited frame on its way to the screen - and, while a screen melt is
// running, the same frame sliding away down the melt's columns.
//
// One shader for both because the melt *is* this blit with a vertical shift:
// `slide` is 0 when nothing is melting, which makes the arithmetic below the
// identity. Two shaders would have had to be kept agreeing on where a pixel
// comes from, and the frame the melt reveals is drawn by the pass underneath it,
// so a half-pixel disagreement between them would show as a seam.
//
// It is in colour rather than palette indices, which is the one place this
// departs from vanilla: the engine slides indices and resolves them through
// whatever palette is current, while this slides a frame that was resolved when
// it was captured. They differ only if the palette changes during the melt - a
// damage flash on the last tic of a level, which the intermission clears - and
// the captured answer is the one that matches what was on the screen.
struct CaptureShader final : ScreenQuadShader
{
    CaptureShader() { compile(); }

    void define() override
    {
        auto corner = quadCorner();
        setQuadPosition(corner);

        auto uv = varying(float2(corner.x(), corner.y()));

        // One offset per two-pixel column of DOOM's own frame, sampled nearest,
        // so the lookup picks the column out on its own with no rounding to do
        // by hand. The melt slides whole frame rows, not whole window pixels -
        // that is the size the melt was in 1993, the same reasoning the fuzz
        // grain gets in ResolveShader.
        auto slid = sample(offsets, float2(uv.x(), 0.5f)).x() * 255.0f * slide;
        auto sourceRow = uv.y() * (float) Engine::screenHeight - slid;

        // Above where the column has slid to, the outgoing frame is gone, and
        // throwing the pixel away is what leaves the incoming one standing.
        setDiscardBelow(sourceRow, 0.0f);

        // Back out to the window, and from there to the capture: the frame
        // occupies dst inside a texture that is the whole view, so a position in
        // points over viewSize is already the normalized coordinate. At slide 0
        // that lands on the texel the fragment came from and the draw is a plain
        // blit.
        auto x = dstOrigin.x() + uv.x() * dstSize.x();
        auto y =
            dstOrigin.y() + sourceRow / (float) Engine::screenHeight * dstSize.y();

        auto texel = sample(capture, float2(x / viewSize.x(), y / viewSize.y()));
        setFragment(float4(texel.xyz(), 1.0f));
    }

    GPU::Uniform<GPU::Texture2D> capture;
    GPU::Uniform<GPU::Texture2D> offsets;

    // 1 while a melt is sliding this frame away, 0 when it is only being drawn.
    GPU::Uniform<GPU::Float> slide;

    EACP_SHADER(viewSize, dstOrigin, dstSize, slide, capture, offsets)
};
} // namespace PureDoom
