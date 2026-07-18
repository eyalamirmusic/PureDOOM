#pragma once

#include "../r_defs.h" // LightTable, SpriteFrame, VisSprite

namespace Doom
{
// Render/Things' private sprite state: the light-table row for the sprite being drawn
// (spritelights), the R_InitSpriteDefs working table and its high-water mark (sprtemp / maxframe),
// the sprite name currently being installed (spritename), and the sink the vissprite pool overflows
// into when a frame has more than MAXVISSPRITES sprites (overflowsprite).
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// Render/Things' own namespace-scope private globals, read by no other file (the cross-read sprite
// tables sprites/numsprites stay in the r_things.cpp shim, as GraphicsData). The vanilla names become
// references onto the members (sprtemp as a reference-to-array). Live frame-golden-covered - every
// sprite the demos draw goes through spritelights and the vissprite pool.
struct SpriteScratch
{
    LightTable** spritelights =
        nullptr; // colormap row for the current sprite's light
    SpriteFrame sprtemp[29] = {}; // R_InitSpriteDefs working frames
    int maxframe = 0; // highest frame index seen while installing
    char* spritename = nullptr; // the sprite name being installed
    VisSprite overflowsprite = {}; // sink for the (MAXVISSPRITES+1)th sprite
};

// The one SpriteScratch, a view onto the Engine's member - the same pattern as the other clusters
// (compositeCache(), wallScratch(), ...).
SpriteScratch& spriteScratch();
} // namespace Doom
