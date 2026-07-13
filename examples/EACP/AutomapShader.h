#pragma once

#include "Common.h"

namespace PureDoom
{
// The automap, drawn as real geometry at the window's resolution rather than
// rasterized into DOOM's 320x168 frame and blown up.
//
// Each map line arrives as the four corners of a quad that has not been widened
// yet: the perpendicular it has to be offset along needs the line's length, and
// normalize is free here, where the engine would have to run its Newton's-method
// square root over every line of the map every frame. The line's colour is a
// palette index the automap picked outright - nothing on the map is textured -
// and it goes through the COLORMAP only to pick up the menu's darkening (see
// eacpDoomDarkenRow), which row 0 leaves alone.
struct AutomapShader final : GPU::ShaderProgram
{
    AutomapShader() { compile(); }

    void define() override
    {
        auto position = vertexInput(&EacpDoomAutomapVertex::position);
        auto direction = vertexInput(&EacpDoomAutomapVertex::direction);
        auto side = vertexInput(&EacpDoomAutomapVertex::side);
        auto color = vertexInput(&EacpDoomAutomapVertex::color);

        auto normal = normalize(float2(-direction.y(), direction.x()));
        auto corner = position + normal * (side * lineWidth * 0.5f);

        auto x = dstOrigin.x() + corner.x() / frameSize.x() * dstSize.x();
        auto y = dstOrigin.y() + corner.y() / frameSize.y() * dstSize.y();
        auto ndcX = x / viewSize.x() * 2.0f - 1.0f;
        auto ndcY = 1.0f - y / viewSize.y() * 2.0f;
        setPosition(float4(ndcX, ndcY, 0.0f, 1.0f));

        auto index = varying(color);
        auto shaded = sample(colormap,
                             float2((index + 0.5f) / 256.0f,
                                    (darkenRow + 0.5f) / colormapRows))
                          .x();
        auto rgb = sample(palette, float2((shaded * 255.0f + 0.5f) / 256.0f, 0.5f));

        setFragment(float4(rgb.xyz(), 1.0f));
    }

    GPU::Uniform<GPU::Float2> viewSize;
    GPU::Uniform<GPU::Float2> dstOrigin;
    GPU::Uniform<GPU::Float2> dstSize;

    // The engine's automap frame, which the geometry is emitted in.
    GPU::Uniform<GPU::Float2> frameSize;

    // In those same frame units: 1.0 is the single pixel vanilla rasterizes.
    GPU::Uniform<GPU::Float> lineWidth;
    GPU::Uniform<GPU::Float> darkenRow;

    GPU::Uniform<GPU::Texture2D> colormap;
    GPU::Uniform<GPU::Texture2D> palette;

    EACP_SHADER(viewSize,
                dstOrigin,
                dstSize,
                frameSize,
                lineWidth,
                darkenRow,
                colormap,
                palette)
};
} // namespace PureDoom
