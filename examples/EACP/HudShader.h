#pragma once

#include "Common.h"
#include "ScreenShader.h"

namespace PureDoom
{
// The weapon in the player's hands, and its muzzle flash. DOOM draws these in
// screen space rather than in the world, so they are a plain quad on the near
// plane, over whatever the world pass left behind — the same masked sprite and
// COLORMAP lookups the world uses, without the perspective.
struct HudShader final : GPU::ShaderProgram
{
    HudShader() { compile(); }

    void define() override
    {
        auto corner = vertexInput(&ScreenVertex::corner);

        auto x = dstOrigin.x() + corner.x() * dstSize.x();
        auto y = dstOrigin.y() + corner.y() * dstSize.y();
        auto ndcX = x / viewSize.x() * 2.0f - 1.0f;
        auto ndcY = 1.0f - y / viewSize.y() * 2.0f;
        setPosition(float4(ndcX, ndcY, 0.0f, 1.0f));

        auto u = uRange.x() + corner.x() * (uRange.y() - uRange.x());
        auto texel = sample(texture, varying(float2(u, corner.y())));
        setDiscardBelow(texel.w(), 0.5f);

        auto index = (texel.x() * 255.0f + 0.5f) / 256.0f;
        auto shaded =
            sample(colormap, float2(index, (light + 0.5f) / colormapRows)).x();

        // The weapon darkens with the world it is held in front of; row 0 is the
        // identity, so it only does so while the menu is up.
        auto darkened = sample(colormap,
                               float2((shaded * 255.0f + 0.5f) / 256.0f,
                                      (darkenRow + 0.5f) / colormapRows))
                            .x();

        auto color =
            sample(palette, float2((darkened * 255.0f + 0.5f) / 256.0f, 0.5f));

        setFragment(float4(color.xyz(), 1.0f));
    }

    GPU::Uniform<GPU::Float2> viewSize;
    GPU::Uniform<GPU::Float2> dstOrigin;
    GPU::Uniform<GPU::Float2> dstSize;

    // The sprite's horizontal source range: {0, 1}, or {1, 0} for the frames
    // the engine marks as mirrored.
    GPU::Uniform<GPU::Float2> uRange;
    GPU::Uniform<GPU::Float> light;
    GPU::Uniform<GPU::Float> darkenRow;

    GPU::Uniform<GPU::Texture2D> texture;
    GPU::Uniform<GPU::Texture2D> colormap;
    GPU::Uniform<GPU::Texture2D> palette;

    EACP_SHADER(viewSize,
                dstOrigin,
                dstSize,
                uRange,
                light,
                darkenRow,
                texture,
                colormap,
                palette)
};
} // namespace PureDoom
