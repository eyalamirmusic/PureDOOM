#pragma once

#include "Common.h"

namespace PureDoom
{
// Draws the level geometry the engine access layer extracts, with DOOM's map
// coordinates (x, y ground plane, z up) mapped to GPU space as (x, z, -y).
// The full-frame perspective projection is then squeezed into the 3D
// viewport's sub-rect of the window (offsets scale by w so they survive the
// perspective divide), leaving the status bar strip and the letterbox bars
// untouched.
//
// Shading reproduces the software renderer exactly, as two chained lookups on
// data the engine already owns: the wall or flat texture yields a palette
// index, the COLORMAP row picked by the surface's light level and its distance
// remaps that index to a darker one, and the palette turns it into a colour.
// The banding, the palette flashes and the light diminishing are DOOM's own,
// not an approximation of them.
struct WorldShader final : GPU::ShaderProgram
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

        // Masked textures - every sprite, and the walls with holes in them -
        // carry their coverage in alpha; the empty pixels are thrown away here,
        // so what is behind them shows through instead of being occluded. A
        // plain indexed texture has no alpha channel and reads as 1, so it
        // never loses a pixel.
        setDiscardBelow(texel.w(), 0.5f);

        auto index = texel.x();

        // The engine's light table, in closed form: a surface starts at the
        // COLORMAP row its sector's brightness picks and moves one row darker
        // as it recedes. Sampling nearest and clamped, the texture's own edges
        // do the rounding and the clamping.
        auto row = startMap - constant(1280.0f) / (depth + constant(16.0f));
        auto shaded =
            sample(colormap,
                   float2(paletteCoordinate(index), (row + 0.5f) / colormapRows))
                .x();

        // The menu darkens what is behind it by running the finished frame
        // through a COLORMAP row, which is a second lookup here rather than a
        // pass over 64000 pixels there - and leaves the world at full resolution
        // under the menu instead of replacing it with the software frame. Row 0
        // is the identity, so playing costs the lookup and nothing else.
        auto darkened = sample(colormap,
                               float2(paletteCoordinate(shaded),
                                      (darkenRow + 0.5f) / colormapRows))
                            .x();

        auto color = sample(palette, float2(paletteCoordinate(darkened), 0.5f));
        setFragment(float4(color.xyz(), 1.0f));
    }

    // A palette index arrives from an R8 texture as 0..1; this lands it on the
    // centre of its texel in a 256-wide lookup row.
    GPU::Float paletteCoordinate(const GPU::Float& index)
    {
        return (index * 255.0f + 0.5f) / 256.0f;
    }

    GPU::Uniform<GPU::Float> camX;
    GPU::Uniform<GPU::Float> camY;
    GPU::Uniform<GPU::Float> camZ;
    GPU::Uniform<GPU::Float> yaw;
    GPU::Uniform<GPU::Float2> ndcScale;
    GPU::Uniform<GPU::Float2> ndcOffset;
    GPU::Uniform<GPU::Float> darkenRow;

    // Rebound per draw: the frame's geometry is grouped by texture, so each
    // wall texture and flat is drawn in one run.
    GPU::Uniform<GPU::Texture2D> texture;
    GPU::Uniform<GPU::Texture2D> colormap;
    GPU::Uniform<GPU::Texture2D> palette;

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
