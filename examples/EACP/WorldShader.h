#pragma once

#include "DoomShader.h"

namespace PureDoom
{
// The level as hardware 3D, at the window's resolution. DOOM's map coordinates
// (x, y on the ground, z up) arrive as (x, z, -y), and the full-frame projection
// is squeezed into the 3D viewport's sub-rect of the window - offsets scale by w
// so they survive the perspective divide - leaving the status bar and the
// letterbox bars alone.
struct WorldShader final : DoomShader
{
    WorldShader() { compile(); }

    void define() override
    {
        auto position = vertexInput(&EacpDoomVertex::position);
        auto uv = vertexInput(&EacpDoomVertex::uv);
        auto light = vertexInput(&EacpDoomVertex::light);

        auto view = rotateY(-yaw) * translate(-camX, -camY, -camZ);
        auto fovY = 2.0f * std::atan(1.0f / worldAspect);
        auto projection = perspective(constant(worldAspect), fovY, 4.0f, 16384.0f);
        auto clip = projection * view * float4(position, 1.0f);

        auto x = clip.x() * ndcScale.x() + clip.w() * ndcOffset.x();
        auto y = clip.y() * ndcScale.y() + clip.w() * ndcOffset.y();
        setPosition(float4(x, y, clip.z(), clip.w()));

        // The projection's w is the view depth, so the distance the light
        // falloff needs comes free with the transform.
        auto depth = varying(clip.w());
        auto startMap = varying(light);
        auto texel = sample(texture, varying(uv));

        // Sprites and the wall textures with holes in them carry their coverage
        // in alpha; a plain indexed texture has none and reads as 1, so it never
        // loses a pixel here.
        setDiscardBelow(texel.w(), 0.5f);

        // A surface starts at the COLORMAP row its sector's brightness picks and
        // moves one row darker as it recedes - the engine's light table, in
        // closed form.
        auto row = startMap - constant(1280.0f) / (depth + constant(16.0f));
        setPaletteFragment(darkened(remap(indexOf(texel), row)));
    }

    GPU::Uniform<GPU::Float> camX;
    GPU::Uniform<GPU::Float> camY;
    GPU::Uniform<GPU::Float> camZ;
    GPU::Uniform<GPU::Float> yaw;
    GPU::Uniform<GPU::Float2> ndcScale;
    GPU::Uniform<GPU::Float2> ndcOffset;

    // Rebound per draw: the frame's geometry is grouped by texture.
    GPU::Uniform<GPU::Texture2D> texture;

    EACP_SHADER(camX,
                camY,
                camZ,
                yaw,
                ndcScale,
                ndcOffset,
                darkenRow,
                texture,
                colormap,
                palette)
};
} // namespace PureDoom
