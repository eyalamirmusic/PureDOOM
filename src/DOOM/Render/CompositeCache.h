#pragma once

#include "../doomtype.h" // byte

namespace Doom
{
// The renderer's texture-composition working data - what R_InitData / R_GenerateComposite build and
// R_GetColumn reads: the PNAMES patch-directory bounds (firstpatch / lastpatch / numpatches) and the
// last flat, and the per-composed-texture column cache (texturewidthmask, texturecompositesize, the
// texturecolumnlump / texturecolumnofs column tables, and texturecomposite itself, the lazily
// generated column bytes), plus the flat/texture/sprite memory counters R_PrecacheLevel tallies.
// Distinct from GraphicsData (the loaded-once WAD graphics, read-only after R_InitData): this is the
// composition machinery over them, generated lazily and mutated as columns are first drawn.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// Render/Data's own namespace-scope private globals, read by no other file (the r_data.cpp shim
// re-exports the cross-read GraphicsData names, not these). The vanilla names become references onto
// the members. Live frame-golden-covered - every wall column the demos draw goes through the
// composite cache - so the byte-identical goldens are a live confirmation, not just build + app-link.
struct CompositeCache
{
    int lastflat = 0; // last flat lump (firstflat/numflats are in GraphicsData)
    int firstpatch = 0; // first PNAMES patch lump
    int lastpatch = 0; // last PNAMES patch lump
    int numpatches = 0; // # of patches

    int* texturewidthmask = nullptr; // per-texture width-1 tiling mask
    int* texturecompositesize = nullptr; // per-texture composite byte size
    short** texturecolumnlump = nullptr; // per-column source lump (or -1 = composed)
    unsigned short** texturecolumnofs = nullptr; // per-column byte offset
    byte** texturecomposite = nullptr; // per-texture composed column bytes (lazy)

    int flatmemory = 0; // bytes of flats precached
    int texturememory = 0; // bytes of textures precached
    int spritememory = 0; // bytes of sprites precached
};

// The one CompositeCache, a view onto the Engine's member - the same pattern as the other clusters
// (graphicsData(), renderScratch(), ...).
CompositeCache& compositeCache();
} // namespace Doom
