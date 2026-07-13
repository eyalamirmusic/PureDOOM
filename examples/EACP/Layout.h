#pragma once

#include "Common.h"

namespace PureDoom
{
// Snaps a proposed content size back to DOOM's 4:3 shape by applying the
// smaller of the two possible corrections, so dragging any window edge or
// corner feels natural.
inline void keepDisplayAspect(int& width, int& height)
{
    constexpr auto aspect = displayWidth / displayHeight;

    auto heightFromWidth = (float) width / aspect;
    auto widthFromHeight = (float) height * aspect;

    if (std::abs(heightFromWidth - (float) height)
        <= std::abs(widthFromHeight - (float) width))
        height = (int) std::lround(heightFromWidth);
    else
        width = (int) std::lround(widthFromHeight);
}

inline Graphics::WindowOptions windowOptions()
{
    auto options = Graphics::WindowOptions {};
    options.width = (int) displayWidth * windowScale;
    options.height = (int) displayHeight * windowScale;
    options.title = "Pure DOOM (eacp)";
    options.minWidth = (int) displayWidth;
    options.minHeight = (int) displayHeight;
    options.onWillResize = keepDisplayAspect;
    return options;
}

// The largest centered 4:3 rect that fits the view, in view points; the
// window's aspect constraint keeps this a no-op except during zoom and
// fullscreen, where black bars fill the rest.
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
