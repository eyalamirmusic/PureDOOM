#pragma once

#include <eacp/GPU/GPU.h>

#include <array>
#include <cmath>
#include <cstdint>

#include <DOOM/DOOM.h>

#include "EngineAccess.h"

// The engine's live palette, including the damage, pickup and invulnerability
// flashes, which are palette swaps. DOOM.h does not declare it in its public
// section. No extern "C": the engine is C++ now, and i_video.cpp defines this
// with C++ linkage. (It would have linked either way - a namespace-scope variable
// is not mangled - but the declaration would have been a lie.)
extern unsigned char screen_palette[256 * 3];

namespace PureDoom
{
using namespace eacp;

constexpr auto doomWidth = 320;
constexpr auto doomHeight = 200;

// The software frame splits into the 3D view and the status bar below it -
// unless the player sizes the bar away, when the view is the whole frame
// (eacpDoomViewRows).
constexpr auto viewRowsWithStatusBar = 168.0f;

// The rows a view of that height occupies out of the 240 the frame is displayed
// as, once the 1.2 CRT stretch is applied: 84% of it with the status bar up.
inline float worldViewportShare(float rows)
{
    return rows * 1.2f / 240.0f;
}

// DOOM's horizontal field of view is 90 degrees, and its vertical one follows
// from how tall the view stands on the display. The camera's projection is built
// for the view with the status bar up; a taller view wants a wider vertical
// field, which is one more scale on the projected y rather than another
// projection (View::drawWorld).
constexpr auto worldAspect = (float) doomWidth / (viewRowsWithStatusBar * 1.2f);

// Ceilings for one frame of geometry; a shareware level fills a small fraction.
constexpr auto maxVertices = 262144;
constexpr auto maxDraws = 2048;
constexpr auto maxAutomapVertices = 131072;

constexpr auto automapWidth = (float) EACP_DOOM_AUTOMAP_WIDTH;
constexpr auto automapHeight = (float) EACP_DOOM_AUTOMAP_HEIGHT;

// In those same frame units: 1.0 is the single pixel vanilla rasterizes.
constexpr auto automapLineWidth = 1.0f;

constexpr auto screenPixels = EACP_DOOM_SCREEN_WIDTH * EACP_DOOM_SCREEN_HEIGHT;

// The captured overlay: a palette index and its coverage, per pixel.
constexpr auto overlayBytes = screenPixels * 4;

constexpr auto colormapRows = (float) EACP_DOOM_COLORMAP_ROWS;
constexpr auto pi = 3.14159265358979f;

// DOOM's 320x200 frame was drawn for 4:3 CRTs, whose non-square pixels stretched
// it 1.2x vertically; 320x240 is the shape it was meant to be seen at.
constexpr auto displayWidth = 320.0f;
constexpr auto displayHeight = 240.0f;

// eacp reports no display metrics (see the gap log), so the initial size is a
// guess that fits a laptop screen; the window resizes from there.
constexpr auto windowScale = 3;
} // namespace PureDoom
