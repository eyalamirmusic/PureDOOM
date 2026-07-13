#pragma once

#include <eacp/GPU/GPU.h>

#include <array>
#include <cmath>
#include <cstdint>

#include "../../PureDOOM.h"

#include "EngineAccess.h"

// The engine's current palette (RGB triplets), kept up to date by
// I_SetPalette — including the damage/pickup/invulnerability flashes, which
// are palette swaps. Defined in DoomImpl.c; PureDOOM.h doesn't declare it in
// its public section.
extern "C" unsigned char screen_palette[256 * 3];

namespace PureDoom
{
using namespace eacp;

constexpr auto doomWidth = 320;
constexpr auto doomHeight = 200;

// The software frame splits into the 3D view (168 rows) and the status bar
// (32 rows); with the 1.2 CRT stretch the view fills exactly 84% of the
// displayed height, and its displayed aspect (320 : 201.6) fixes the GPU
// camera's field of view: DOOM's horizontal FOV is 90 degrees.
constexpr auto viewRows = 168.0f;
constexpr auto worldViewportShare = viewRows * 1.2f / 240.0f;
constexpr auto worldAspect = 320.0f / (viewRows * 1.2f);

// Ceilings for one frame of world geometry; a shareware level fills a small
// fraction of either.
constexpr auto maxVertices = 262144;
constexpr auto maxDraws = 2048;

// Six vertices per map line, and a level's walls, grid and things together stay
// far below this.
constexpr auto maxAutomapVertices = 131072;

// The automap's frame, and the width it draws its lines at in that frame's
// units: vanilla rasterizes a single pixel.
constexpr auto automapWidth = (float) EACP_DOOM_AUTOMAP_WIDTH;
constexpr auto automapHeight = (float) EACP_DOOM_AUTOMAP_HEIGHT;
constexpr auto automapLineWidth = 1.0f;

// The captured overlay: a palette index and its coverage, per pixel of the
// engine's frame.
constexpr auto overlayBytes =
    (std::size_t) EACP_DOOM_SCREEN_WIDTH * EACP_DOOM_SCREEN_HEIGHT * 4;

constexpr auto colormapRows = (float) EACP_DOOM_COLORMAP_ROWS;
constexpr auto pi = 3.14159265358979f;

// The engine's tics per second: the rate its whole world moves at.
constexpr auto ticRate = 35.0;

// DOOM's 320x200 frame was designed for 4:3 CRTs, whose non-square pixels
// stretched it 1.2x vertically; 320x240 is the intended display shape.
constexpr auto displayWidth = 320.0f;
constexpr auto displayHeight = 240.0f;

// eacp has no display-metrics API yet (see the gap log), so the initial size
// is a guess that fits laptop screens; the window resizes from there.
constexpr auto windowScale = 3;

constexpr auto mouseSpeed = 4.0f;
} // namespace PureDoom
