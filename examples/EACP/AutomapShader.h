#pragma once

#include "DoomShader.h"

namespace PureDoom
{
// The map as geometry rather than as a rasterized frame, so its lines keep their
// real endpoints instead of snapping to whole pixels.
//
// A line arrives as the four corners of a quad that has not been widened yet:
// widening needs the line's length, and the GPU normalizes for free, where the
// engine would have to run its Newton's-method square root over every line of the
// map every frame. The colour is a palette index the automap picked outright -
// nothing on the map is textured - and it goes through the COLORMAP only to pick
// up the menu's darkening.
struct AutomapShader final : ScreenQuadShader
{
    AutomapShader() { compile(); }

    void define() override
    {
        auto position = vertexInput(&Engine::AutomapVertex::position);
        auto direction = vertexInput(&Engine::AutomapVertex::direction);
        auto side = vertexInput(&Engine::AutomapVertex::side);
        auto color = vertexInput(&Engine::AutomapVertex::color);

        auto normal = normalize(float2(-direction.y(), direction.x()));
        auto corner = position + normal * (side * lineWidth * 0.5f);

        setViewPosition(dstOrigin.x() + corner.x() / frameSize.x() * dstSize.x(),
                        dstOrigin.y() + corner.y() / frameSize.y() * dstSize.y());

        setPaletteFragment(darkened(varying(color)));
    }

    // The engine's automap frame, which the geometry is emitted in.
    GPU::Uniform<GPU::Float2> frameSize;
    GPU::Uniform<GPU::Float> lineWidth;

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
