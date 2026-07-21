#pragma once

#include <eacp/GPU/GPU.h>

#include <array>
#include <cmath>
#include <cstdint>

#include <DOOM/DOOM.h>

#include "EngineAccess.h"

namespace PureDoom
{
using namespace eacp;

// The software frame splits into the 3D view and the status bar below it -
// unless the player sizes the bar away, when the view is the whole frame
// (Engine::viewRows).
constexpr auto viewRowsWithStatusBar = 168.0f;

// DOOM's 320x200 frame was drawn for 4:3 CRTs, whose non-square pixels stretched
// it 1.2x vertically; 320x240 is the shape it was meant to be seen at.
constexpr auto crtStretch = 1.2f;
constexpr auto displayWidth = (float) Engine::screenWidth;
constexpr auto displayHeight = (float) Engine::screenHeight * crtStretch;

// The rows a view of that height occupies out of the 240 the frame is displayed
// as: 84% of it with the status bar up.
inline float worldViewportShare(float rows)
{
    return rows * crtStretch / displayHeight;
}

// DOOM's horizontal field of view is 90 degrees, and its vertical one follows
// from how tall the view stands on the display. The camera's projection is built
// for the view with the status bar up; a taller view wants a wider vertical
// field, which is one more scale on the projected y rather than another
// projection (View::drawWorld).
constexpr auto worldAspect = displayWidth / (viewRowsWithStatusBar * crtStretch);

// Ceilings for one frame of geometry; a shareware level fills a small fraction.
constexpr auto maxVertices = 262144;
constexpr auto maxDraws = 2048;
constexpr auto maxAutomapVertices = 131072;

// In the automap's own frame units: 1.0 is the single pixel vanilla rasterizes.
constexpr auto automapLineWidth = 1.0f;

// The captured overlay: a palette index and its coverage, per pixel.
constexpr auto overlayBytes = Engine::screenPixels * 4;

constexpr auto pi = 3.14159265358979f;

// eacp reports no display metrics (see the gap log), so the initial size is a
// guess that fits a laptop screen; the window resizes from there.
constexpr auto windowScale = 3;
} // namespace PureDoom
