#pragma once

#include "DoomShader.h"

namespace PureDoom
{
// The layers the engine draws over the view in software and nothing else
// reproduces - HUD messages, the level name, the PAUSE graphic, the menu, the
// automap's marks - laid back over the GPU view.
//
// They arrive as palette indices with their coverage in alpha, so the pixels the
// engine never drew on are thrown away and the world shows through at its own
// resolution instead of being replaced by a 320x200 copy of itself.
//
// A menu darkens the frame it finds and then draws itself over it, so a message
// or the level's name is already on the screen by then and dims with the world,
// while the menu itself stays bright. Green says which, and picks the row.
struct OverlayShader final : ScreenQuadShader
{
    OverlayShader() { compile(); }

    void define() override
    {
        auto corner = quadCorner();
        setQuadPosition(corner);

        auto texel = sample(overlay, varying(float2(corner.x(), corner.y())));
        setDiscardBelow(texel.w(), 0.5f);

        setPaletteFragment(remap(indexOf(texel), darkenRow * texel.y()));
    }

    GPU::Uniform<GPU::Texture2D> overlay;

    EACP_SHADER(viewSize, dstOrigin, dstSize, darkenRow, overlay, colormap, palette)
};
} // namespace PureDoom
