#pragma once

#include "../doomtype.h" // byte

#include <ea_data_structures/Structures/Vector.h>

namespace Doom
{
// The renderer's texture-composition working data - what Doom::initData / Doom::generateComposite build and
// Doom::getColumn reads: the last flat, and the per-composed-texture column cache (texturewidthmask, texturecompositesize, the
// texturecolumnlump / texturecolumnofs column tables, and texturecomposite itself, the lazily
// generated column bytes).
// Distinct from GraphicsData (the loaded-once WAD graphics, read-only after Doom::initData): this is the
// composition machinery over them, generated lazily and mutated as columns are first drawn.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// Render/Data's own namespace-scope private globals, read by no other file (the r_data.cpp shim
// re-exports the cross-read GraphicsData names, not these). The scalars were references onto
// the members until the file-local-alias sweep (REFACTOR.md, Step 9 strand (a)) retired them -
// initFlats and precacheLevel, the only toucher of lastflat, hoists compositeCache() once.
// firstpatch/lastpatch/numpatches went further: retiring their aliases showed nothing read them and
// nothing had ever filled them in, so the members are deleted too, the way Step 5 deleted
// viewangleoffset/linecount/loopcount. flatmemory/texturememory/spritememory went the same way in a
// later audit: accumulated by initFlats/precacheLevel but read nowhere, in vanilla too, so the
// counters themselves are gone and only the doom_memcpy/cacheLumpNum work that fills the cache
// remains (matching AutomapView::min_w/min_h and WeaponScratch::swingx/swingy). The composition
// tables below are a separate mechanism - RAII-owned vectors with plain-pointer views, not
// references - and are untouched by either sweep. Live frame-golden-covered - every wall column the
// demos draw goes through the composite cache - so the byte-identical goldens are a live
// confirmation, not just build + app-link.
struct CompositeCache
{
    int lastflat = 0; // last flat lump (firstflat/numflats are in GraphicsData)

    // The per-texture composition tables, RAII-owned (Step 9) - what were raw never-freed
    // doom_malloc pointers. The vanilla names in Data.cpp are plain-pointer VIEWS onto these,
    // refreshed after initTextures sizes them (the same owner/view split GraphicsData's arrays
    // and Level's geometry use). texturewidthmask / texturecompositesize are flat int arrays;
    // the three T** tables are nested owners - an inner vector per texture holds the bytes and a
    // pointer array is the T** the view points at (as GraphicsData's textureStorage/
    // texturePointers do for textures[]).
    EA::Vector<int> texturewidthmask; // per-texture width-1 tiling mask
    EA::Vector<int> texturecompositesize; // per-texture composite byte size

    EA::Vector<EA::Vector<short>>
        columnlumpStorage; // per-texture: per-column source lump
    EA::Vector<short*> columnlump; // the short** view (texturecolumnlump)
    EA::Vector<EA::Vector<unsigned short>>
        columnofsStorage; // per-texture: per-column offset
    EA::Vector<unsigned short*>
        columnofs; // the unsigned short** view (texturecolumnofs)

    // The lazily-composed column bytes. Each inner vector carries the load-bearing 64-byte zero
    // tail for the renderer's tutti-frutti over-read (REFACTOR.md Step 4), value-initialised to
    // zero as the old doom_malloc + doom_memset(0) block was. composite is the byte** view; a
    // null entry means "not composed yet" (getColumn's regenerate check).
    EA::Vector<EA::Vector<byte>> compositeStorage;
    EA::Vector<byte*> composite; // the byte** view (texturecomposite)
};

// The one CompositeCache, a view onto the Engine's member - the same pattern as the other clusters
// (graphicsData(), renderScratch(), ...).
CompositeCache& compositeCache();
} // namespace Doom
