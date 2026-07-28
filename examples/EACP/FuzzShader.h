#pragma once

#include "DoomShader.h"

namespace PureDoom
{
// A spectre, which DOOM does not draw: drawVisSprite hands a thing with
// MF_SHADOW to drawFuzzColumn, which fills the sprite's shape with the pixels
// already in the frame behind it, one row up or down, remapped through COLORMAP
// row 6. So all this shader draws is the shape - a mark in the world target's
// green channel - and ResolveShader does the rest.
//
// It is drawn additively, which is what leaves the index the world wrote in red
// exactly as it was: those indices *are* the fuzz, and overwriting them with a
// silhouette would leave nothing to distort. Depth is the world's, tested
// normally, so a spectre behind a wall is not marked - hence its draws go last,
// after everything the frame is made of is in the target.
struct FuzzShader final : WorldViewShader
{
    FuzzShader() { compile(); }

    void define() override
    {
        auto position = vertexInput(&Engine::WorldVertex::position);
        auto uv = vertexInput(&Engine::WorldVertex::uv);

        setWorldPosition(position);

        // The one thing the sprite is read for. Its light, which the vertex
        // carries like every other, is not: a fuzz pixel takes its brightness
        // from row 6 and the frame beneath it, whatever the sector is lit to.
        auto texel = sample(texture, varying(uv));
        setDiscardBelow(texel.w(), 0.5f);

        setFragment(float4(constant(0.0f), 1.0f, 0.0f, 1.0f));
    }

    EACP_SHADER(camX, camY, camZ, yaw, ndcScale, ndcOffset, texture)
};
} // namespace PureDoom
