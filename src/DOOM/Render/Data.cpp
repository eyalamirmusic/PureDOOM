// Rewritten out of vanilla r_data into namespace Doom.
//
// Doom::Texture/flat/sprite/colormap data: compose a wall texture from its patches (what a
// non-software renderer must do too), build the column lookup, retrieve a column,
// and load the TEXTURE/PNAMES/flat/sprite/COLORMAP lumps. r_data.cpp shims the R_
// names; the externally-read tables (textures, colormaps, spritewidth, ...) live
// there, while r_data's own state (the composite cache, patch/flat counts, memory
// counters) is file-local here. The tutti-frutti over-read in getColumn is
// load-bearing and preserved (REFACTOR.md Step 4).

#include "../Host/Platform.h"

#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"
#include "../Math/Swap.h"
#include "../Sim/SimDefs.h"
#include "../Wad/WadFile.h"

#include "../Game/SkyState.h"
#include "../Sim/ThinkerList.h"
#include "CompositeCache.h"
#include "Data.h"
#include "GraphicsData.h"

#include <cstddef>
#include <ea_data_structures/Structures/Array.h>
#include <ea_data_structures/Structures/Vector.h>

#include "../Host/System.h"
#include "../Game/DemoState.h"
#include "../Sim/Level.h"
namespace Doom
{
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
// The lumps are referenced by number, and patched
// into the rectangular texture space using origin
// and possibly other attributes.
//
struct MapPatch
{
    short originx;
    short originy;
    short patch;
    short stepdir;
    short colormap;
};

//
// Texture definition.
// A DOOM wall texture is a list of patches
// which are to be combined in a predefined order.
//
struct MapTexture
{
    char name[8];
    // An ON-DISK field, not an application boolean: this struct is overlaid directly
    // onto raw TEXTURE1/TEXTURE2 lump bytes. The value is never read, but its four
    // bytes are load-bearing - as a one-byte bool, width/height/columndirectory/
    // patchcount below would all shift by three and every texture in every WAD would
    // parse as garbage. It was declared int ahead of the engine-wide flip to bool
    // precisely so that flip could not reach it, and it stays int for the same reason.
    int masked;
    short width;
    short height;
    //void **columndirectory; // OBSOLETE
    int columndirectory; // [pd] If it's not used, at least make sure it's the right size! Pointers are 8 bytes in x64
    short patchcount;
    MapPatch patches[1];
};

// initTextures decodes lump bytes through these types (by aligned copy - see
// there), so their layout must equal the on-disk one: 10-byte patch records
// starting 22 bytes into each texture record.
static_assert(sizeof(MapPatch) == 10);
static_assert(offsetof(MapTexture, patches) == 22);

// r_data's own state - the per-texture composite cache and patch/flat bookkeeping - now lives on
// the Engine (Render/CompositeCache.h, moved by the file-scope-statics sweep - REFACTOR.md, Step
// 5); read by nothing else. lastflat was a reference onto that member until the file-local-alias
// sweep (REFACTOR.md, Step 9 strand (a)) retired it - initFlats and precacheLevel each hoist
// compositeCache() once. firstpatch/lastpatch/numpatches were references too, but nothing in this
// file (or anywhere else) ever read them, so they were simply dropped rather than hoisted; the
// fields still live on CompositeCache for anyone who wants to fill them in again. flatmemory/
// texturememory/spritememory were dropped the same way in a later audit: accumulated below but
// read nowhere, in vanilla too, so only the cacheLumpNum calls that actually fill the cache remain.
// The composition tables are RAII-owned by CompositeCache now (Step 9); these are plain-pointer
// VIEWS onto the owners' data(), refreshed by initTextures once the vectors are sized (which is
// before any read - generateLookup runs from initTextures, generateComposite/getColumn at render
// time). texturecolumnlump/ofs/composite point at the pointer-array views, so name[tex][col] and
// name[tex] = ... resolve and write through unchanged.
static int* texturewidthmask = nullptr;
static int* texturecompositesize = nullptr;
static short** texturecolumnlump = nullptr;
static unsigned short** texturecolumnofs = nullptr;
static byte** texturecomposite = nullptr;

// Forward declarations so call order needs no rearranging.
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

void drawColumnInCache(Column* patch, byte* cache, int originy, int cacheheight)
{
    int count;
    int position;
    byte* source;

    while (patch->topdelta != 0xff)
    {
        source = reinterpret_cast<byte*>(patch) + 3;
        count = patch->length;
        position = originy + patch->topdelta;

        if (position < 0)
        {
            count += position;
            position = 0;
        }

        if (position + count > cacheheight)
            count = cacheheight - position;

        if (count > 0)
            doom_memcpy(cache + position, source, count);

        patch = reinterpret_cast<Column*>(reinterpret_cast<byte*>(patch)
                                          + patch->length + 4);
    }
}

//
// generateComposite
// Using the texture definition,
//  the composite texture is created from the patches,
//  and each column is cached.
//
void generateComposite(int texnum)
{
    byte* block;
    Texture* texture;
    TexPatch* patch;
    Patch* realpatch;
    int x;
    int x1;
    int x2;
    int i;
    Column* patchcol;
    short* collump;
    unsigned short* colofs;

    texture = textures[texnum];

    // A 64-byte zero tail, as WadFile gives each lump, so the renderer's
    // tutti-frutti over-read past a composited column draws a deterministic zero
    // rather than whatever heap follows (the zone's arena made it deterministic
    // for free; malloc does not). assign value-initialises the whole block to zero
    // for the same reason - drawColumnInCache fills only the covered columns. RAII
    // now (Step 9): the block is a CompositeCache-owned inner vector, freed with the
    // Engine; the byte** view points at its data().
    auto& compositeBytes = compositeCache().compositeStorage[texnum];
    compositeBytes.assign(texturecompositesize[texnum] + 64, byte(0));
    block = compositeBytes.data();
    texturecomposite[texnum] = block;

    collump = texturecolumnlump[texnum];
    colofs = texturecolumnofs[texnum];

    // Composite the columns together.
    patch = texture->patches.data();

    for (i = 0, patch = texture->patches.data(); i < texture->patchcount;
         i++, patch++)
    {
        realpatch = static_cast<Patch*>(Doom::cacheLumpNum(patch->patch));
        x1 = patch->originx;
        x2 = x1 + littleEndian(realpatch->width);

        if (x1 < 0)
            x = 0;
        else
            x = x1;

        if (x2 > texture->width)
            x2 = texture->width;

        for (; x < x2; x++)
        {
            // Column does not have multiple patches?
            if (collump[x] >= 0)
                continue;

            patchcol = reinterpret_cast<Column*>(
                reinterpret_cast<byte*>(realpatch)
                + littleEndian(realpatch->columnofs[x - x1]));
            drawColumnInCache(
                patchcol, block + colofs[x], patch->originy, texture->height);
        }
    }

    // The block is a plain owning allocation now, never purged, so there is no
    // Z_ChangeTag to PU_CACHE and getColumn's regenerate-if-purged branch never
    // re-fires (texturecomposite[texnum] stays set once built).
}

//
// generateLookup
//
void generateLookup(int texnum)
{
    Texture* texture;
    TexPatch* patch;
    Patch* realpatch;
    int x;
    int x1;
    int x2;
    int i;
    short* collump;
    unsigned short* colofs;

    texture = textures[texnum];

    // Composited texture not created yet.
    texturecomposite[texnum] = nullptr;

    texturecompositesize[texnum] = 0;
    collump = texturecolumnlump[texnum];
    colofs = texturecolumnofs[texnum];

    // Now count the number of columns
    //  that are covered by more than one patch.
    // Fill in the lump / offset, so columns
    //  with only a single patch are all done.
    // RAII scratch: value-initialised to zero and released on every exit, including
    // the early "column without a patch" return below (which the manual free leaked).
    auto patchcount = EA::Vector<byte>(texture->width);
    patch = texture->patches.data();

    for (i = 0, patch = texture->patches.data(); i < texture->patchcount;
         i++, patch++)
    {
        realpatch = static_cast<Patch*>(Doom::cacheLumpNum(patch->patch));
        x1 = patch->originx;
        x2 = x1 + littleEndian(realpatch->width);

        if (x1 < 0)
            x = 0;
        else
            x = x1;

        if (x2 > texture->width)
            x2 = texture->width;
        for (; x < x2; x++)
        {
            patchcount[x]++;
            collump[x] = patch->patch;
            colofs[x] = littleEndian(realpatch->columnofs[x - x1]) + 3;
        }
    }

    for (x = 0; x < texture->width; x++)
    {
        if (!patchcount[x])
        {
            //doom_print("generateLookup: column without a patch (%s)\n",
            //    texture->name);
            print("generateLookup: column without a patch (",
                  nameView(texture->name.data(), 8),
                  ")\n");
            return;
        }
        // fatalError ("generateLookup: column without a patch");

        if (patchcount[x] > 1)
        {
            // Use the cached block.
            collump[x] = -1;
            colofs[x] = texturecompositesize[texnum];

            if (texturecompositesize[texnum] > 0x10000 - texture->height)
            {
                //fatalError("Error: generateLookup: texture %i is >64k",
                //        texnum);

                fatalError("Error: generateLookup: texture ", texnum, " is >64k");
            }

            texturecompositesize[texnum] += texture->height;
        }
    }
}

//
// getColumn
//
byte* getColumn(int tex, int col)
{
    int lump;
    int ofs;

    col &= texturewidthmask[tex];
    lump = texturecolumnlump[tex][col];
    ofs = texturecolumnofs[tex][col];

    if (lump > 0)
        return static_cast<byte*>(Doom::cacheLumpNum(lump)) + ofs;

    if (!texturecomposite[tex])
        generateComposite(tex);

    return texturecomposite[tex] + ofs;
}

//
// initTextures
// Initializes the texture list
//  with the textures from the world map.
//
void initTextures()
{
    Texture* texture;
    TexPatch* patch;

    int i;
    int j;

    int* maptex;
    int* maptex2;
    int* maptex1;

    char* names;
    char* name_p;

    int nummappatches;
    int offset;
    int maxoff;
    int maxoff2;
    int numtextures1;
    int numtextures2;

    int* directory;

    int temp1;
    int temp2;
    int temp3;

    auto& gd = graphicsData();

    // Load the patch names from pnames.lmp.
    names = static_cast<char*>(Doom::cacheLumpName("PNAMES"));
    nummappatches = littleEndian(*(reinterpret_cast<int*>(names)));
    name_p = names + 4;
    auto patchlookup = EA::Vector<int>(nummappatches);

    for (i = 0; i < nummappatches; i++)
        patchlookup[i] = Doom::wad().find(nameView(name_p + i * 8, 8));

    // Load the map texture definitions from textures.lmp.
    // The data is contained in one or two lumps,
    //  TEXTURE1 for shareware, plus TEXTURE2 for commercial.
    maptex = maptex1 = static_cast<int*>(Doom::cacheLumpName("TEXTURE1"));
    numtextures1 = littleEndian(*maptex);
    maxoff = Doom::wad().length(Doom::wad().number("TEXTURE1"));
    directory = maptex + 1;

    if (Doom::wad().find("TEXTURE2") != -1)
    {
        maptex2 = static_cast<int*>(Doom::cacheLumpName("TEXTURE2"));
        numtextures2 = littleEndian(*maptex2);
        maxoff2 = Doom::wad().length(Doom::wad().number("TEXTURE2"));
    }
    else
    {
        maptex2 = 0;
        numtextures2 = 0;
        maxoff2 = 0;
    }
    gd.numtextures = numtextures1 + numtextures2;

    // GraphicsData owns the texture structs by value now (RAII, Step 9); `textures`
    // stays a Texture** view onto the texturePointers array into that storage, so
    // every textures[i]->field reader is unchanged. Sized once here (stable after - the
    // loop below only fills them, never resizes).
    gd.textureStorage.resize(gd.numtextures);
    gd.texturePointers.resize(gd.numtextures);
    textures = gd.texturePointers.data();

    // The composition tables are CompositeCache-owned EA::Vectors now (Step 9); size them once
    // here and point the views at their data(). columnlump/ofs/composite own an inner vector per
    // texture (filled below / lazily); composite is null-initialised, which getColumn keys on.
    auto& cc = compositeCache();
    cc.columnlumpStorage.resize(gd.numtextures);
    cc.columnlump.resize(gd.numtextures);
    cc.columnofsStorage.resize(gd.numtextures);
    cc.columnofs.resize(gd.numtextures);
    cc.compositeStorage.resize(gd.numtextures);
    cc.composite.resize(gd.numtextures);
    cc.texturecompositesize.resize(gd.numtextures);
    cc.texturewidthmask.resize(gd.numtextures);

    texturecolumnlump = cc.columnlump.data();
    texturecolumnofs = cc.columnofs.data();
    texturecomposite = cc.composite.data();
    texturecompositesize = cc.texturecompositesize.data();
    texturewidthmask = cc.texturewidthmask.data();

    // textureheight and texturetranslation are GraphicsData-owned too (Step 9); views
    // onto data() refreshed after each resize.
    gd.textureheight.resize(gd.numtextures);
    textureheight = gd.textureheight.data();

    // Really complex printing shit...
    temp1 = Doom::wad().number("S_START"); // P_???????
    temp2 = Doom::wad().number("S_END") - 1;
    temp3 = ((temp2 - temp1 + 63) / 64) + ((gd.numtextures + 63) / 64);
    print("[");
    for (i = 0; i < temp3; i++)
        print(" ");
    print("         ]");
    for (i = 0; i < temp3; i++)
        print("\x8");
    print("\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8");

    for (i = 0; i < gd.numtextures; i++, directory++)
    {
        if (!(i & 63))
            print(".");

        if (i == numtextures1)
        {
            // Start looking in second texture file.
            maptex = maptex2;
            maxoff = maxoff2;
            directory = maptex + 1;
        }

        offset = littleEndian(*directory);

        if (offset > maxoff)
            fatalError("Error: initTextures: bad texture directory");

        // The record sits at whatever byte offset the directory names, which is
        // usually not aligned for MapTexture's int members (BIGDOOR1's record
        // sits at offset % 4 == 2, and UBSan flagged every read through an
        // overlaid pointer). So the record is decoded through aligned copies -
        // the header first, then each patch record. Tests/Sim/TextureTests.cpp
        // holds the decode against the lump's own bytes.
        const byte* rawTexture = reinterpret_cast<byte*>(maptex) + offset;

        MapTexture mtexture;
        doom_memcpy(&mtexture, rawTexture, offsetof(MapTexture, patches));

        // The struct lives in textureStorage now; point the view entry at it and size
        // its patches vector (RAII, Step 9) instead of the old variable-length malloc.
        texture = &gd.textureStorage[i];
        gd.texturePointers[i] = texture;

        texture->width = littleEndian(mtexture.width);
        texture->height = littleEndian(mtexture.height);
        texture->patchcount = littleEndian(mtexture.patchcount);
        texture->patches.resize(texture->patchcount);

        doom_memcpy(texture->name.data(), mtexture.name, sizeof(texture->name));
        const byte* rawPatch = rawTexture + offsetof(MapTexture, patches);
        patch = &texture->patches[0];

        for (j = 0; j < texture->patchcount;
             j++, rawPatch += sizeof(MapPatch), patch++)
        {
            MapPatch mpatch;
            doom_memcpy(&mpatch, rawPatch, sizeof(MapPatch));

            patch->originx = littleEndian(mpatch.originx);
            patch->originy = littleEndian(mpatch.originy);
            patch->patch = patchlookup[littleEndian(mpatch.patch)];
            if (patch->patch == -1)
            {
                //fatalError("Error: initTextures: Missing patch in texture %s",
                //        texture->name);

                fatalError("Error: initTextures: Missing patch in texture ",
                           nameView(texture->name.data(), 8));
            }
        }
        cc.columnlumpStorage[i].resize(texture->width);
        cc.columnofsStorage[i].resize(texture->width);
        texturecolumnlump[i] = cc.columnlumpStorage[i].data();
        texturecolumnofs[i] = cc.columnofsStorage[i].data();

        j = 1;
        while (j * 2 <= texture->width)
            j <<= 1;

        texturewidthmask[i] = j - 1;
        textureheight[i] = Doom::Fixed::fromInt(texture->height);
    }

    // Precalculate whatever possible.
    for (i = 0; i < gd.numtextures; i++)
        generateLookup(i);

    // Create translation table for global animation.
    gd.texturetranslation.resize(gd.numtextures + 1);
    texturetranslation = gd.texturetranslation.data();

    for (i = 0; i < gd.numtextures; i++)
        texturetranslation[i] = i;
}

//
// initFlats
//
void initFlats()
{
    auto& gd = graphicsData();
    auto& cc = compositeCache();

    gd.firstflat = Doom::wad().number("F_START") + 1;
    cc.lastflat = Doom::wad().number("F_END") - 1;
    gd.numflats = cc.lastflat - gd.firstflat + 1;

    // Create translation table for global animation. GraphicsData owns it (Step 9);
    // flattranslation is a view onto data() (P_ animation writes through it).
    gd.flattranslation.resize(gd.numflats + 1);
    flattranslation = gd.flattranslation.data();

    for (int i = 0; i < gd.numflats; i++)
        flattranslation[i] = i;
}

//
// initSpriteLumps
// Finds the width and hoffset of all sprites in the wad,
//  so the sprite does not need to be cached completely
//  just for having the header info ready during rendering.
//
void initSpriteLumps()
{
    Patch* patch;

    auto& gd = graphicsData();

    gd.firstspritelump = Doom::wad().number("S_START") + 1;
    gd.lastspritelump = Doom::wad().number("S_END") - 1;

    gd.numspritelumps = gd.lastspritelump - gd.firstspritelump + 1;

    // GraphicsData owns these now (RAII, Step 9); the vanilla names are plain-pointer
    // views onto the vectors' data(), refreshed here after the resize - the same
    // owner/view split as Level's geometry (numvertexes / vertexes).
    gd.spritewidth.resize(gd.numspritelumps);
    gd.spriteoffset.resize(gd.numspritelumps);
    gd.spritetopoffset.resize(gd.numspritelumps);
    spritewidth = gd.spritewidth.data();
    spriteoffset = gd.spriteoffset.data();
    spritetopoffset = gd.spritetopoffset.data();

    for (int i = 0; i < gd.numspritelumps; i++)
    {
        if (!(i & 63))
            print(".");

        patch = static_cast<Patch*>(Doom::cacheLumpNum(gd.firstspritelump + i));
        spritewidth[i] = Doom::Fixed::fromInt(littleEndian(patch->width));
        spriteoffset[i] = Doom::Fixed::fromInt(littleEndian(patch->leftoffset));
        spritetopoffset[i] = Doom::Fixed::fromInt(littleEndian(patch->topoffset));
    }
}

//
// initColormaps
//
void initColormaps()
{
    int lump, length;

    // Load in the light tables,
    //  256 byte align tables.
    // GraphicsData owns the backing buffer now (RAII, Step 9); colormaps is the
    // 256-byte-aligned view into it, as the original doom_malloc(length) + align was.
    lump = Doom::wad().number("COLORMAP");
    length = Doom::wad().length(lump) + 255;
    auto& gd = graphicsData();
    gd.colormapStorage.resize(length);
    colormaps = reinterpret_cast<LightTable*>(
        (reinterpret_cast<unsigned long long>(gd.colormapStorage.data()) + 255)
        & ~0xffULL);
    Doom::wad().read(lump, colormaps);
}

//
// initData
// Locates all the lumps
//  that will be used by all views
// Must be called after W_Init.
//
void initData()
{
    initTextures();
    print("\nInitTextures");
    initFlats();
    print("\nInitFlats");
    initSpriteLumps();
    print("\nInitSprites");
    initColormaps();
    print("\nInitColormaps");
}

//
// flatNumForName
// Retrieval, get a flat number for a flat name.
//
int flatNumForName(std::string_view name)
{
    int i = Doom::wad().find(name);

    if (i == -1)
    {
        //fatalError("Error: flatNumForName: %s not found", namet);
        fatalError("Error: flatNumForName: ", name, " not found");
    }
    return i - graphicsData().firstflat;
}

// What doom_strncasecmp(stored, name, 8) == 0 answered: case-insensitive over
// the eight columns, the stored name zero-padded when short. A NUL in either
// place has to meet a NUL in the other.
static bool textureNameMatches(const EA::Array<char, 8>& stored,
                               std::string_view name)
{
    for (int i = 0; i < 8; ++i)
    {
        auto wanted = i < static_cast<int>(name.size()) ? name[i] : char(0);

        if (toUpper(stored[i]) != toUpper(wanted))
            return false;

        if (wanted == 0)
            return true;
    }

    return true;
}

//
// checkTextureNumForName
// Check whether texture is available.
// Filter out NoTexture indicator.
//
int checkTextureNumForName(std::string_view name)
{
    // "NoTexture" marker.
    if (!name.empty() && name.front() == '-')
        return 0;

    auto& gd = graphicsData();

    for (int i = 0; i < gd.numtextures; i++)
        if (textureNameMatches(textures[i]->name, name))
            return i;

    return -1;
}

//
// textureNumForName
// Calls checkTextureNumForName,
//  aborts with error message.
//
int textureNumForName(std::string_view name)
{
    int i = checkTextureNumForName(name);

    if (i == -1)
    {
        //fatalError("Error: textureNumForName: %s not found",
        //        name);

        fatalError("Error: textureNumForName: ", name, " not found");
    }
    return i;
}

//
// precacheLevel
// Preloads all relevant graphics for the level.
//
void precacheLevel()
{
    int i;
    int j;
    int k;
    int lump;

    Texture* texture;
    Doom::Thinker* th;
    SpriteFrame* sf;

    if (demoState().demoplayback)
        return;

    auto& gd = graphicsData();

    // Precache flats.
    auto flatpresent = EA::Vector<char>(gd.numflats);

    for (i = 0; i < numsectors; i++)
    {
        flatpresent[sectors[i].floorpic] = 1;
        flatpresent[sectors[i].ceilingpic] = 1;
    }

    for (i = 0; i < gd.numflats; i++)
    {
        if (flatpresent[i])
        {
            lump = gd.firstflat + i;
            Doom::cacheLumpNum(lump);
        }
    }

    // Precache textures.
    auto texturepresent = EA::Vector<char>(gd.numtextures);

    for (i = 0; i < numsides; i++)
    {
        texturepresent[sides[i].toptexture] = 1;
        texturepresent[sides[i].midtexture] = 1;
        texturepresent[sides[i].bottomtexture] = 1;
    }

    // Sky texture is always present.
    // Note that F_SKY1 is the name used to
    //  indicate a sky floor/ceiling as a flat,
    //  while the sky texture is stored like
    //  a wall texture, with an episode dependend
    //  name.
    texturepresent[skyState().skytexture] = 1;

    for (i = 0; i < gd.numtextures; i++)
    {
        if (!texturepresent[i])
            continue;

        texture = textures[i];

        for (j = 0; j < texture->patchcount; j++)
        {
            lump = texture->patches[j].patch;
            Doom::cacheLumpNum(lump);
        }
    }

    // Precache sprites.
    auto spritepresent = EA::Vector<char>(gd.numsprites);

    auto& cap = thinkerList().cap;

    for (th = cap.next; th != &cap; th = th->next)
    {
        if (th->kind() == Doom::ThinkerKind::Mobj && !th->removed)
            spritepresent[static_cast<int>(reinterpret_cast<Mobj*>(th)->sprite)] = 1;
    }

    for (i = 0; i < gd.numsprites; i++)
    {
        if (!spritepresent[i])
            continue;

        for (j = 0; j < sprites[i].numframes; j++)
        {
            sf = &sprites[i].spriteframes[j];
            for (k = 0; k < 8; k++)
            {
                lump = gd.firstspritelump + sf->lump[k];
                Doom::cacheLumpNum(lump);
            }
        }
    }
}
} // namespace Doom

// ---------------------------------------------------------------------------
// Global-scope data that was r_data.cpp. It stays at :: scope because these are the
// vanilla names other translation units (and the eacp port) still link against.
// ---------------------------------------------------------------------------
// The renderer's loaded graphics data (textures, flats, sprite lumps, colormaps) is a
// Doom::GraphicsData owned by the Engine now; these vanilla names are references onto it.
// R_InitData fills the members once at startup; they are read-only after.

// A Doom::Texture** view onto GraphicsData's owned texturePointers array (Step 9);
// R_InitTextures points it at data() after the resize.
Doom::Texture** textures = nullptr;

// needed for texture pegging. A view onto GraphicsData's owned EA::Vector (Step 9);
// initTextures points it at data() after the resize.
fixed_t* textureheight = nullptr;

// for global animation. Views onto GraphicsData's owned EA::Vectors (Step 9), set to
// data() by initTextures / initFlats; P_ animation writes through them.
int* flattranslation = nullptr;
int* texturetranslation = nullptr;

// needed for pre rendering. Plain-pointer views onto GraphicsData's owned EA::Vectors
// (Step 9); initSpriteLumps points them at data() after filling the vectors.
fixed_t* spritewidth = nullptr;
fixed_t* spriteoffset = nullptr;
fixed_t* spritetopoffset = nullptr;

// A 256-byte-aligned view into GraphicsData's owned colormapStorage; initColormaps
// points it at the aligned offset after reading the COLORMAP lump (Step 9).
Doom::LightTable* colormaps = nullptr;
