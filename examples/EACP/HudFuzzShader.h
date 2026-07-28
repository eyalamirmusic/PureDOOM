#pragma once

#include "DoomShader.h"

namespace PureDoom
{
// The weapon while the invisibility sphere is up, which DOOM fuzzes exactly as
// it fuzzes a spectre.
//
// It is marked into the world target rather than drawn over the screen, which
// is what lets it share the spectres' machinery down to the last texel: a
// weapon is a quad in front of the world, and the world is what a fuzz pixel
// reads. So this is HudShader's geometry with FuzzShader's output, and the
// resolve does not need to know which of the two put the mark there.
struct HudFuzzShader final : ScreenQuadShader
{
    HudFuzzShader()
    {
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

        setFragment(float4(constant(0.0f), 1.0f, 0.0f, 1.0f));
    }

    // {0, 1}, or {1, 0} for the frames the engine marks as mirrored.
    GPU::Uniform<GPU::Float2> uRange;
    GPU::Uniform<GPU::Texture2D> texture;

    EACP_SHADER(viewSize, dstOrigin, dstSize, uRange, texture)
};
} // namespace PureDoom
