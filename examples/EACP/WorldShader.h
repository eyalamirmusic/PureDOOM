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
    WorldShader()
    {
        // Wall textures and flats tile across a surface, so they repeat. A floor
        // needs it most: its UVs are world coordinates over 64, running to
        // hundreds, where clamping would sample one texel and draw the whole
        // surface a single flat colour.
        //
        // Declared here rather than on the Texture because the sampler is fixed
        // when the shader compiles - see GPU::TextureSampling. Set before
        // compile(), which is what reads it.
        texture.sampling = {GPU::TextureFilter::Nearest,
                            GPU::TextureAddressMode::Repeat};

        compile();
    }

    void define() override
    {
        auto position = vertexInput(&EacpDoomVertex::position);
        auto uv = vertexInput(&EacpDoomVertex::uv);
        auto light = vertexInput(&EacpDoomVertex::light);
        auto falloff = vertexInput(&EacpDoomVertex::falloff);

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
