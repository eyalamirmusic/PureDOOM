#pragma once

#include "../r_data.h" // texture_t, and spritedef_t / lighttable_t / fixed_t through it

#include <ea_data_structures/Structures/Vector.h>

namespace Doom
{
// The graphics the renderer loads from the WAD once at startup and then only reads:
// the composed wall textures and their heights, the flats, the sprite lumps and their
// pre-measured dimensions, the sprite frame table, and the COLORMAP. R_InitData builds
// all of it (R_InitTextures / R_InitFlats / R_InitSpriteLumps / R_InitColormaps, and
// R_InitSprites for the frame table), the wall/plane/sprite drawers read it every
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
    // (for pegging), and the animation redirect R_UpdateAnimations walks.
    int numtextures = 0;
    texture_t** textures = nullptr;
    fixed_t* textureheight = nullptr;
    int* texturetranslation = nullptr;

    // Flats: the first flat lump, the flat count, and the flat animation redirect.
    int firstflat = 0;
    int numflats = 0;
    int* flattranslation = nullptr;

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

    // Sprite frames: the per-sprite frame/rotation table R_InitSprites builds.
    int numsprites = 0;
    spritedef_t* sprites = nullptr;

    // The COLORMAP lump: the base every light row (see Lighting) indexes into.
    lighttable_t* colormaps = nullptr;
};

// The one GraphicsData, a view onto the Engine's member - the same pattern as
// lighting(), viewWindow(), viewProjection(), viewPoint(), clip(), level(), wad() and
// randomness().
GraphicsData& graphicsData();
} // namespace Doom
