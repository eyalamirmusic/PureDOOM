#pragma once

#include "Common.h"

namespace PureDoom
{
struct ScreenVertex
{
    float corner[2] = {};
};

inline constexpr ScreenVertex unitQuad[] = {
    {{0.0f, 0.0f}},
    {{1.0f, 0.0f}},
    {{1.0f, 1.0f}},
    {{0.0f, 0.0f}},
    {{1.0f, 1.0f}},
    {{0.0f, 1.0f}},
};

// Every pixel the port draws resolves the way the software renderer resolves
// one: a palette index is remapped by the COLORMAP row that the surface's light
// and distance pick, and the palette turns the result into a colour. Indices
// travel as 0..255 floats, so a texture's 0..1 sample is scaled back up on the
// way in and a lookup row's sample on the way out.
//
// Both lookup tables are read with fetch rather than sample: a table is
// addressed by an index the shader already holds, and fetch takes that index as
// one, so the half-texel arithmetic that used to stand between the two - and
// the Nearest sampler that had to round back to the texel it was aimed at - is
// gone. What the sampler was silently supplying is written out below instead.
struct DoomShader : GPU::ShaderProgram
{
    GPU::Float indexOf(const GPU::Float4& texel) { return texel.x() * 255.0f; }

    // fetch truncates towards zero, so this is where the rounding lives now. An
    // index arrives as a unorm scaled back up, which lands a hair either side of
    // the whole number it means.
    GPU::Int texelOf(const GPU::Float& index) { return toInt(index + 0.5f); }

    GPU::Float remap(const GPU::Float& index, const GPU::Float& row)
    {
        // The row is a continuous falloff, and at close range it runs tens of
        // rows off the bright end of the table - WorldShader subtracts up to 64
        // of them at the near plane. A texel outside the texture fetches as zero,
        // where the Clamp address mode used to hold it at the first row, so the
        // clamp is the shader's to make now. It bounds the table rather than the
        // light levels: rows 32 and 33 are the invulnerability and blackout
        // maps, which a surface locked to one of them arrives here already
        // carrying.
        auto bounded = clamp(row, 0.0f, (float) Engine::colormapRows - 1.0f);
        return indexOf(fetch(colormap, int2(texelOf(index), texelOf(bounded))));
    }

    // Row 0 is the identity, so playing costs the lookup and nothing else.
    GPU::Float darkened(const GPU::Float& index) { return remap(index, darkenRow); }

    void setPaletteFragment(const GPU::Float& index)
    {
        auto color = fetch(palette, int2(texelOf(index), 0));
        setFragment(float4(color.xyz(), 1.0f));
    }

    GPU::Uniform<GPU::Float> darkenRow;
    GPU::Uniform<GPU::Texture2D> colormap;
    GPU::Uniform<GPU::Texture2D> palette;
};

// A quad drawn over a destination rect in view points, which the unit quad's
// corner is mapped onto and then into clip space.
struct ScreenQuadShader : DoomShader
{
    GPU::Float2 quadCorner() { return vertexInput(&ScreenVertex::corner); }

    void setQuadPosition(const GPU::Float2& corner)
    {
        setViewPosition(dstOrigin.x() + corner.x() * dstSize.x(),
                        dstOrigin.y() + corner.y() * dstSize.y());
    }

    void setViewPosition(const GPU::Float& x, const GPU::Float& y)
    {
        auto ndcX = x / viewSize.x() * 2.0f - 1.0f;
        auto ndcY = 1.0f - y / viewSize.y() * 2.0f;
        setPosition(float4(ndcX, ndcY, 0.0f, 1.0f));
    }

    void setDestination(const Graphics::Rect& bounds, const Graphics::Rect& dst)
    {
        viewSize = std::array {bounds.w, bounds.h};
        dstOrigin = std::array {dst.x, dst.y};
        dstSize = std::array {dst.w, dst.h};
    }

    GPU::Uniform<GPU::Float2> viewSize;
    GPU::Uniform<GPU::Float2> dstOrigin;
    GPU::Uniform<GPU::Float2> dstSize;
};
} // namespace PureDoom
