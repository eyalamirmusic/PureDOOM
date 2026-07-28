#pragma once

#include "DoomShader.h"

namespace PureDoom
{
// The level's surfaces - walls, floors, ceilings, the sky and every thing in it
// as a billboard - drawn into the world target as palette indices rather than as
// colours (DoomShader::setIndexFragment). The camera and the projection are
// WorldViewShader's; what is here is the texture and the light.
struct WorldShader final : WorldViewShader
{
    WorldShader() { compile(); }

    void define() override
    {
        auto position = vertexInput(&Engine::WorldVertex::position);
        auto uv = vertexInput(&Engine::WorldVertex::uv);
        auto light = vertexInput(&Engine::WorldVertex::light);
        auto falloff = vertexInput(&Engine::WorldVertex::falloff);

        auto depth = varying(setWorldPosition(position));
        auto startMap = varying(light);
        auto recedes = varying(falloff);
        auto texel = sample(texture, varying(uv));

        // Sprites and the wall textures with holes in them carry their coverage
        // in alpha; a plain indexed texture has none and reads as 1, so it never
        // loses a pixel here.
        setDiscardBelow(texel.w(), 0.5f);

        // A surface starts at the COLORMAP row its sector's brightness picks and
        // moves one row darker as it recedes - the engine's light table, in
        // closed form. A surface the engine locks to a single row - the sky, a
        // lit sprite frame, anything seen through the invulnerability sphere or
        // the light-amp visor - carries a falloff of zero and keeps the row it
        // came in with.
        auto row =
            startMap - recedes * (constant(1280.0f) / (depth + constant(16.0f)));

        setIndexFragment(remap(indexOf(texel), row));
    }

    EACP_SHADER(camX, camY, camZ, yaw, ndcScale, ndcOffset, texture, colormap)
};
} // namespace PureDoom
