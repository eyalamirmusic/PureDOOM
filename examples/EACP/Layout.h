#pragma once

#include "Common.h"

namespace PureDoom
{
inline Graphics::WindowOptions windowOptions()
{
    auto options = Graphics::WindowOptions {};
    options.width = (int) displayWidth * windowScale;
    options.height = (int) displayHeight * windowScale;
    options.title = "Pure DOOM (eacp)";
    options.minWidth = (int) displayWidth;
    options.minHeight = (int) displayHeight;

    // The constraint governs resizing but is not retro-fitted to the size asked
    // for above, which is already 4:3 because windowScale multiplies both.
    options.aspectRatio = Graphics::Point {displayWidth, displayHeight};
    return options;
}

// The largest centered 4:3 rect that fits the view, in view points. The window's
// aspect constraint keeps this a no-op except during zoom and fullscreen, where
// black bars fill the rest.
inline Graphics::Rect letterboxedDisplayRect(const Graphics::Rect& bounds)
{
    constexpr auto contentAspect = displayWidth / displayHeight;

    if (bounds.w <= 0.0f || bounds.h <= 0.0f)
        return bounds;

    auto width = bounds.h * contentAspect;

    if (width <= bounds.w)
        return {(bounds.w - width) / 2.0f, 0.0f, width, bounds.h};

    auto height = bounds.w / contentAspect;
    return {0.0f, (bounds.h - height) / 2.0f, bounds.w, height};
}
} // namespace PureDoom
