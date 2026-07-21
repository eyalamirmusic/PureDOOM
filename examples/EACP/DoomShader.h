#pragma once

#include "Common.h"

namespace PureDoom
{
struct ScreenVertex
{
    float corner[2];
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
struct DoomShader : GPU::ShaderProgram
{
    GPU::Float indexOf(const GPU::Float4& texel) { return texel.x() * 255.0f; }

    GPU::Float indexCoordinate(const GPU::Float& index)
    {
        return (index + 0.5f) / 256.0f;
    }

    GPU::Float remap(const GPU::Float& index, const GPU::Float& row)
    {
        auto uv = float2(indexCoordinate(index),
                         (row + 0.5f) / (float) Engine::colormapRows);
        return indexOf(sample(colormap, uv));
    }

    // Row 0 is the identity, so playing costs the lookup and nothing else.
    GPU::Float darkened(const GPU::Float& index) { return remap(index, darkenRow); }

    void setPaletteFragment(const GPU::Float& index)
    {
        auto color = sample(palette, float2(indexCoordinate(index), 0.5f));
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
