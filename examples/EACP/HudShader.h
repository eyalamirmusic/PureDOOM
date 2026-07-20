#pragma once

#include "DoomShader.h"

namespace PureDoom
{
// The weapon in the player's hands and its muzzle flash, which DOOM draws in
// screen space rather than in the world: the world's sprite and COLORMAP lookups
// without the perspective.
struct HudShader final : ScreenQuadShader
{
    HudShader()
    {
        // The weapon sprites go up through makeWorldTexture like every other
        // world texture, so they repeat for consistency; a HUD sprite's own UVs
        // stay inside [0,1], so nothing here depends on it.
        texture.sampling = {GPU::TextureFilter::Nearest,
                            GPU::TextureAddressMode::Repeat};

        compile();
    }

    void define() override
    {
        auto corner = quadCorner();
        setQuadPosition(corner);

        auto u = uRange.x() + corner.x() * (uRange.y() - uRange.x());
        auto texel = sample(texture, varying(float2(u, corner.y())));
        setDiscardBelow(texel.w(), 0.5f);

        setPaletteFragment(darkened(remap(indexOf(texel), light)));
    }

    // {0, 1}, or {1, 0} for the frames the engine marks as mirrored.
    GPU::Uniform<GPU::Float2> uRange;
    GPU::Uniform<GPU::Float> light;
    GPU::Uniform<GPU::Texture2D> texture;

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
