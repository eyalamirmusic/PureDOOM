#pragma once

#include "../Sim/MapTypes.h"
#include "RenderTypes.h" // LightTable, SpriteFrame, VisSprite

#include <ea_data_structures/Structures/Array.h>

#include <string_view>

namespace Doom
{
// Render/Things' private sprite state: the light-table row for the sprite being drawn
// (spritelights), the R_InitSpriteDefs working table and its high-water mark (sprtemp / maxframe),
// the sprite name currently being installed (spritename), and the sink the vissprite pool overflows
// into when a frame has more than SpriteState::maxVisSprites sprites (overflowsprite).
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// Render/Things' own namespace-scope private globals, read by no other file (the cross-read sprite
// tables sprites/numsprites stay in the r_things.cpp shim, as GraphicsData). sprtemp, spritelights,
// maxframe, spritename and overflowsprite were all references onto the member (sprtemp as a
// reference-to-array) until the file-local-alias sweep (REFACTOR.md, Step 9 strand (a)) retired
// them - each toucher in Render/Things hoists spriteScratch() once and reaches them through it. Live
// frame-golden-covered - every sprite the demos draw goes through spritelights and the vissprite pool.
struct SpriteScratch
{
    LightTable** spritelights =
        nullptr; // colormap row for the current sprite's light
    EA::Array<SpriteFrame, 29> sprtemp = {}; // R_InitSpriteDefs working frames
    int maxframe = 0; // highest frame index seen while installing
    std::string_view spritename; // the sprite name being installed
    VisSprite overflowsprite = {}; // sink for the (maxVisSprites + 1)th sprite
};

// The one SpriteScratch, a view onto the Engine's member - the same pattern as the other clusters
// (compositeCache(), wallScratch(), ...).
SpriteScratch& spriteScratch();
} // namespace Doom
