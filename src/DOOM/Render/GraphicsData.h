#pragma once

#include "../r_data.h" // Texture, and SpriteDef / lighttable_t / fixed_t through it

#include <ea_data_structures/Structures/Vector.h>

namespace Doom
{
// The graphics the renderer loads from the WAD once at startup and then only reads:
// the composed wall textures and their heights, the flats, the sprite lumps and their
// pre-measured dimensions, the sprite frame table, and the COLORMAP. Doom::initData builds
// all of it (Doom::initTextures / Doom::initFlats / Doom::initSpriteLumps / Doom::initColormaps, and
// Doom::initSprites for the frame table), the wall/plane/sprite drawers read it every
// column, and the GPU port uploads straight from it.
//
// The fifth scalar cluster off the loose globals into the Engine (REFACTOR.md, Step 5),
// and the first that is loaded data rather than per-frame state - so it is init-once and
// read-only after, not written each tic. Every entry is a plain pointer or count (the
// tables are malloc'd, not fixed arrays), so the vanilla names are simple references
// onto these members; the storage moves off the r_data.cpp / r_things.cpp file-scope
// globals and every reader - the renderer, and the app's EngineAccess / shaders for
// textures / colormaps / spritewidth / sprites - resolves unchanged. Nothing here is
// hashed, so gathering it is golden-neutral, as the earlier clusters were.
struct GraphicsData
{
    // Wall textures: the composed texture table and its count, each texture's height
    // (for pegging), and the animation redirect R_UpdateAnimations walks. RAII-owned
    // (Step 9): textureStorage owns the Texture structs by value (each owning its
    // own patches vector); texturePointers is the Texture* array that the vanilla
    // name `textures` (a Texture** view, r_data.cpp) points at, so readers using
    // textures[i]->field are unchanged. Both sized once by Doom::initTextures.
    int numtextures = 0;
    EA::Vector<Texture> textureStorage;
    EA::Vector<Texture*> texturePointers;
    EA::Vector<fixed_t>
        textureheight; // RAII-owned (Step 9); r_data's name is a view
    EA::Vector<int>
        texturetranslation; // RAII-owned (Step 9); animation writes through the view

    // Flats: the first flat lump, the flat count, and the flat animation redirect.
    int firstflat = 0;
    int numflats = 0;
    EA::Vector<int>
        flattranslation; // RAII-owned (Step 9); animation writes through the view

    // Sprite lumps: the lump range they occupy, and each lump's width and offsets,
    // measured once so the renderer need not re-read the patch headers. RAII-owned
    // (Step 9): the arrays are EA::Vectors here, and the vanilla names (r_data.cpp)
    // are plain-pointer views onto data(), refreshed by initSpriteLumps after the fill
    // - the same owner/view split Level's geometry uses. The rest of this cluster is
    // still raw doom_malloc pointers, pending the same conversion.
    int firstspritelump = 0;
    int lastspritelump = 0;
    int numspritelumps = 0;
    EA::Vector<fixed_t> spritewidth;
    EA::Vector<fixed_t> spriteoffset;
    EA::Vector<fixed_t> spritetopoffset;

    // Sprite frames: the per-sprite frame/rotation table Doom::initSprites builds.
    // RAII-owned (Step 9): the sprite table is an EA::Vector of spritedef_ts, each
    // owning its own frames vector; the vanilla name `sprites` (r_things.cpp) is a
    // plain-pointer view onto data(), refreshed by R_InitSpriteDefs.
    int numsprites = 0;
    EA::Vector<SpriteDef> sprites;

    // The COLORMAP lump: the base every light row (see Lighting) indexes into.
    // RAII-owned (Step 9): this vector is the backing buffer, and the vanilla name
    // `colormaps` (r_data.cpp) is a 256-byte-aligned VIEW into its data() that
    // initColormaps sets after reading the lump. The +255 slack for that alignment is
    // part of this buffer's length, as the original doom_malloc(length + 255) was.
    EA::Vector<lighttable_t> colormapStorage;
};

// The one GraphicsData, a view onto the Engine's member - the same pattern as
// lighting(), viewWindow(), viewProjection(), viewPoint(), clip(), level(), wad() and
// randomness().
GraphicsData& graphicsData();
} // namespace Doom
