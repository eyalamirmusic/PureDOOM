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

// The world is rendered into a texture rather than onto the screen, so its
// pipelines have to name that texture's format instead of the drawable's - and
// a texture pass is single-sampled whatever the view is (GPU::Frame::beginPass).
constexpr auto worldTargetFormat = GPU::TextureFormat::RGBA8Unorm;
constexpr auto worldTargetSamples = 1;

// The finished frame is composited into a texture of its own before it reaches
// the screen, so that when a melt starts the frame it is sliding away is still
// somewhere at the window's resolution rather than only in the engine's 320x200
// copy of it (View::render).
//
// Unlike the world target this one is drawn into by the *same* shaders that draw
// onto the drawable - the resolve, the weapon, the status bar strip, the overlay
// - which are prepared once, for the drawable. So its format is not a choice:
// it has to be the one those pipelines already name.
constexpr auto captureTargetFormat = GPU::TextureFormat::BGRA8Unorm;

static_assert(GPU::pixelFormatFor(captureTargetFormat)
                  == GPU::PixelFormat::BGRA8Unorm,
              "the capture is drawn into by shaders prepared for the drawable, "
              "whose colour format is ShaderProgram::prepare's default");

// Ceilings for one frame of geometry; a shareware level fills a small fraction.
constexpr auto maxVertices = 262144;
constexpr auto maxDraws = 2048;
constexpr auto maxAutomapVertices = 131072;

// In the automap's own frame units: 1.0 is the single pixel vanilla rasterizes.
constexpr auto automapLineWidth = 1.0f;

// The captured overlay: a palette index and its coverage, per pixel.
constexpr auto overlayBytes = Engine::screenPixels * 4;

constexpr auto pi = 3.14159265358979f;

// The largest whole multiple of DOOM's 320x240 the display will take, which is
// the size the window opens at. Whole, because a fractional one puts a texel
// grid on a pixel grid it does not divide into and the shimmer that follows is
// the first thing a player sees; and capped at 4 because past that the window
// is bigger than the screen it was measured on is comfortable with.
constexpr auto maxWindowScale = 4;
} // namespace PureDoom
