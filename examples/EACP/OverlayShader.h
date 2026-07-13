#pragma once

#include "Common.h"
#include "ScreenShader.h"

namespace PureDoom
{
// The layers the engine draws over the view in software and nothing else
// reproduces - HUD messages, the PAUSE graphic, the menu, the automap's marks -
// captured by eacpDoomBuildOverlay and laid back over the GPU view.
//
// They arrive as palette indices with their coverage in alpha, so the pixels the
// engine never drew on are thrown away here and the world shows through them at
// its own resolution instead of being replaced by a 320x200 copy of itself.
//
// A menu darkens the frame it finds and then draws itself over it, so a message,
// the level's name and the PAUSE graphic are already on the screen when that
// happens and dim with the world, while the menu itself stays bright. Which of
// the two a pixel belongs to is in the green channel, and it picks the COLORMAP
// row: the menu's darkening for the one, and row 0 - the identity - for the
// other, and for everything while no menu is up at all.
struct OverlayShader final : GPU::ShaderProgram
{
    OverlayShader() { compile(); }

    void define() override
    {
        auto corner = vertexInput(&ScreenVertex::corner);

        auto x = dstOrigin.x() + corner.x() * dstSize.x();
        auto y = dstOrigin.y() + corner.y() * dstSize.y();
        auto ndcX = x / viewSize.x() * 2.0f - 1.0f;
        auto ndcY = 1.0f - y / viewSize.y() * 2.0f;
        setPosition(float4(ndcX, ndcY, 0.0f, 1.0f));

        auto texel = sample(overlay, varying(float2(corner.x(), corner.y())));
        setDiscardBelow(texel.w(), 0.5f);

        auto index = (texel.x() * 255.0f + 0.5f) / 256.0f;
        auto row = darkenRow * texel.y();
        auto shaded =
            sample(colormap, float2(index, (row + 0.5f) / colormapRows)).x();
        auto color =
            sample(palette, float2((shaded * 255.0f + 0.5f) / 256.0f, 0.5f));

        setFragment(float4(color.xyz(), 1.0f));
    }

    GPU::Uniform<GPU::Float2> viewSize;
    GPU::Uniform<GPU::Float2> dstOrigin;
    GPU::Uniform<GPU::Float2> dstSize;
    GPU::Uniform<GPU::Float> darkenRow;

    GPU::Uniform<GPU::Texture2D> overlay;
    GPU::Uniform<GPU::Texture2D> colormap;
    GPU::Uniform<GPU::Texture2D> palette;

    EACP_SHADER(viewSize, dstOrigin, dstSize, darkenRow, overlay, colormap, palette)
};
} // namespace PureDoom
