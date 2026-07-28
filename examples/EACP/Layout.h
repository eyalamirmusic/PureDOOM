#pragma once

#include "Common.h"

#include <eacp/Graphics/Window/Display.h>

#include <algorithm>

namespace PureDoom
{
// How many times 320x240 fits in what the display will actually give a window,
// with a margin for the title bar the work area does not know about. At least
// one, so a display smaller than DOOM still opens a window rather than none.
inline int windowScale()
{
    const auto work = Graphics::primaryDisplay().workArea;

    const auto horizontal = (int) (work.w * 0.9f / displayWidth);
    const auto vertical = (int) (work.h * 0.9f / displayHeight);

    return std::max(1, std::min({horizontal, vertical, maxWindowScale}));
}

inline Graphics::WindowOptions windowOptions()
{
    const auto scale = windowScale();

    auto options = Graphics::WindowOptions {};
    options.width = (int) displayWidth * scale;
    options.height = (int) displayHeight * scale;
    options.title = "Pure DOOM (eacp)";
    options.minWidth = (int) displayWidth;
    options.minHeight = (int) displayHeight;

    // The constraint governs resizing but is not retro-fitted to the size asked
    // for above, which is already 4:3 because the scale multiplies both.
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
