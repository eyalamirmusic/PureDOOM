#pragma once

#include "../Sim/MapTypes.h"
#include "RenderTypes.h" // Column, byte

#include <ea_data_structures/Structures/Array.h>

// The composed-texture types. Were r_data.h.
namespace Doom
{
struct TexPatch
{
    // Block origin (allways UL),
    // which has allready accounted
    // for the internal origin of the patch.
    int originx;
    int originy;
    int patch;
};
} // namespace Doom

// A maptexturedef_t describes a rectangular texture,
// which is composed of one or more Doom::MapPatch structures
// that arrange graphic patches.
namespace Doom
{
struct Texture
{
    // Keep name for switch changing, etc.
    EA::Array<char, 8> name;
    short width;
    short height;

    // All the patches[patchcount]
    //  are drawn back to front into the cached texture. RAII-owned (Step 9): was a
    //  trailing flexible-array-member (patches[1] with a variable-length malloc);
    //  now an owned vector, so a Texture is fixed-size and frees its own patches.
    //  Readers index it as before (patches[j], &patches[0]).
    short patchcount;
    EA::Vector<TexPatch> patches;
};
} // namespace Doom

// Every wall texture the WAD loaded, and how many.
//
// A texture is a list of patches to be drawn back to front, not a bitmap - which
// is why a masked texture's holes are holes: R_GenerateComposite leaves whatever
// no patch covered. Anything that wants the pixels rather than the engine's
// cached columns (which are post data, not pixels, for exactly those textures)
// has to compose them the same way.
// The texture table lives in Doom::GraphicsData (an Engine member) now. It owns the
// Doom::Texture structs by value; `textures` stays a Doom::Texture** (a view onto a pointer
// array into that storage) so every `textures[i]->field` reader is unchanged (Step 9).
extern Doom::Texture** textures;

// I/O, setting up the stuff.

// Retrieval.
// Floor/ceiling opaque texture tiles,
// lookup by name. For animation?

// Called by Doom::ticker for switches and animations,
// returns the texture number for the texture name.

// How many wall textures and flats the WAD loaded. Composed lazily, so these are
// the id space anything walking the graphics has to work in.

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------

namespace Doom
{
// Renderer data; r_data.cpp keeps the vanilla R_ names as shims.
void drawColumnInCache(Column* patch, byte* cache, int originy, int cacheheight);
void generateComposite(int texnum);
void generateLookup(int texnum);
byte* getColumn(int tex, int col);
void initTextures();
void initFlats();
void initSpriteLumps();
void initColormaps();
void initData();
int flatNumForName(const char* name);
int checkTextureNumForName(const char* name);
int textureNumForName(const char* name);
void precacheLevel();
} // namespace Doom
