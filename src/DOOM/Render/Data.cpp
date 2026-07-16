// Rewritten out of vanilla r_data into namespace Doom.
//
// Texture/flat/sprite/colormap data: compose a wall texture from its patches (what a
// non-software renderer must do too), build the column lookup, retrieve a column,
// and load the TEXTURE/PNAMES/flat/sprite/COLORMAP lumps. r_data.cpp shims the R_
// names; the externally-read tables (textures, colormaps, spritewidth, ...) live
// there, while r_data's own state (the composite cache, patch/flat counts, memory
// counters) is file-local here. The tutti-frutti over-read in getColumn is
// load-bearing and preserved (REFACTOR.md Step 4).

#include "../doom_config.h"

#include "../doomdef.h"
#include "../doomstat.h"
#include "../i_system.h"
#include "../m_swap.h"
#include "../p_local.h"
#include "../r_local.h"
#include "../r_sky.h"
#include "../w_wad.h"

#include <alloca.h>

#include "../r_data.h"
#include "CompositeCache.h"
#include "Data.h"

namespace Doom
{
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
// The lumps are referenced by number, and patched
// into the rectangular texture space using origin
// and possibly other attributes.
//
struct mappatch_t
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
struct maptexture_t
{
    char name[8];
    doom_boolean masked;
    short width;
    short height;
    //void **columndirectory; // OBSOLETE
    int columndirectory; // [pd] If it's not used, at least make sure it's the right size! Pointers are 8 bytes in x64
    short patchcount;
    mappatch_t patches[1];
};

// r_data's own state - the per-texture composite cache, patch/flat bookkeeping and the memory
// counters - now lives on the Engine (Render/CompositeCache.h, moved by the file-scope-statics
// sweep - REFACTOR.md, Step 5). The vanilla names are references onto that member; read by nothing
// else.
static int& lastflat = compositeCache().lastflat;
static int& firstpatch = compositeCache().firstpatch;
static int& lastpatch = compositeCache().lastpatch;
static int& numpatches = compositeCache().numpatches;
static int*& texturewidthmask = compositeCache().texturewidthmask;
static int*& texturecompositesize = compositeCache().texturecompositesize;
static short**& texturecolumnlump = compositeCache().texturecolumnlump;
static unsigned short**& texturecolumnofs = compositeCache().texturecolumnofs;
static byte**& texturecomposite = compositeCache().texturecomposite;
static int& flatmemory = compositeCache().flatmemory;
static int& texturememory = compositeCache().texturememory;
static int& spritememory = compositeCache().spritememory;

// Forward declarations so call order needs no rearranging.
void drawColumnInCache(column_t* patch, byte* cache, int originy, int cacheheight);
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

void drawColumnInCache(column_t* patch, byte* cache, int originy, int cacheheight)
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

        patch = reinterpret_cast<column_t*>(reinterpret_cast<byte*>(patch)
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
    texture_t* texture;
    texpatch_t* patch;
    patch_t* realpatch;
    int x;
    int x1;
    int x2;
    int i;
    column_t* patchcol;
    short* collump;
    unsigned short* colofs;

    texture = textures[texnum];

    // A 64-byte zero tail, as WadFile gives each lump, so the renderer's
    // tutti-frutti over-read past a composited column draws a deterministic zero
    // rather than whatever heap follows (the zone's arena made it deterministic
    // for free; malloc does not). The whole block is zeroed for the same reason -
    // drawColumnInCache fills only the covered columns.
    block = static_cast<byte*>(doom_malloc(texturecompositesize[texnum] + 64));
    doom_memset(block, 0, texturecompositesize[texnum] + 64);
    texturecomposite[texnum] = block;

    collump = texturecolumnlump[texnum];
    colofs = texturecolumnofs[texnum];

    // Composite the columns together.
    patch = texture->patches;

    for (i = 0, patch = texture->patches; i < texture->patchcount; i++, patch++)
    {
        realpatch = static_cast<patch_t*>(W_CacheLumpNum(patch->patch, PU_CACHE));
        x1 = patch->originx;
        x2 = x1 + SHORT(realpatch->width);

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

            patchcol =
                reinterpret_cast<column_t*>(reinterpret_cast<byte*>(realpatch)
                                            + LONG(realpatch->columnofs[x - x1]));
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
    texture_t* texture;
    byte* patchcount; // patchcount[texture->width]
    texpatch_t* patch;
    patch_t* realpatch;
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
    patchcount = static_cast<byte*>(doom_malloc(texture->width));
    doom_memset(patchcount, 0, texture->width);
    patch = texture->patches;

    for (i = 0, patch = texture->patches; i < texture->patchcount; i++, patch++)
    {
        realpatch = static_cast<patch_t*>(W_CacheLumpNum(patch->patch, PU_CACHE));
        x1 = patch->originx;
        x2 = x1 + SHORT(realpatch->width);

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
            colofs[x] = LONG(realpatch->columnofs[x - x1]) + 3;
        }
    }

    for (x = 0; x < texture->width; x++)
    {
        if (!patchcount[x])
        {
            //doom_print("generateLookup: column without a patch (%s)\n",
            //    texture->name);
            doom_print("generateLookup: column without a patch (");
            doom_print(texture->name);
            doom_print(")\n");
            return;
        }
        // I_Error ("generateLookup: column without a patch");

        if (patchcount[x] > 1)
        {
            // Use the cached block.
            collump[x] = -1;
            colofs[x] = texturecompositesize[texnum];

            if (texturecompositesize[texnum] > 0x10000 - texture->height)
            {
                //I_Error("Error: generateLookup: texture %i is >64k",
                //        texnum);

                doom_strcpy(error_buf, "Error: generateLookup: texture ");
                doom_concat(error_buf, doom_itoa(texnum, 10));
                doom_concat(error_buf, " is >64k");
                I_Error(error_buf);
            }

            texturecompositesize[texnum] += texture->height;
        }
    }

    doom_free(patchcount);
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
        return static_cast<byte*>(W_CacheLumpNum(lump, PU_CACHE)) + ofs;

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
    maptexture_t* mtexture;
    texture_t* texture;
    mappatch_t* mpatch;
    texpatch_t* patch;

    int i;
    int j;

    int* maptex;
    int* maptex2;
    int* maptex1;

    char name[9];
    char* names;
    char* name_p;

    int* patchlookup;

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

    // Load the patch names from pnames.lmp.
    name[8] = 0;
    names = static_cast<char*>(W_CacheLumpName("PNAMES", PU_STATIC));
    nummappatches = LONG(*(reinterpret_cast<int*>(names)));
    name_p = names + 4;
    patchlookup =
        static_cast<int*>(doom_malloc(nummappatches * sizeof(*patchlookup)));

    for (i = 0; i < nummappatches; i++)
    {
        doom_strncpy(name, name_p + i * 8, 8);
        patchlookup[i] = W_CheckNumForName(name);
    }

    // Load the map texture definitions from textures.lmp.
    // The data is contained in one or two lumps,
    //  TEXTURE1 for shareware, plus TEXTURE2 for commercial.
    maptex = maptex1 = static_cast<int*>(W_CacheLumpName("TEXTURE1", PU_STATIC));
    numtextures1 = LONG(*maptex);
    maxoff = W_LumpLength(W_GetNumForName("TEXTURE1"));
    directory = maptex + 1;

    if (W_CheckNumForName("TEXTURE2") != -1)
    {
        maptex2 = static_cast<int*>(W_CacheLumpName("TEXTURE2", PU_STATIC));
        numtextures2 = LONG(*maptex2);
        maxoff2 = W_LumpLength(W_GetNumForName("TEXTURE2"));
    }
    else
    {
        maptex2 = 0;
        numtextures2 = 0;
        maxoff2 = 0;
    }
    numtextures = numtextures1 + numtextures2;

    textures =
        static_cast<texture_t**>(doom_malloc(numtextures * sizeof(texture_t*)));
    texturecolumnlump =
        static_cast<short**>(doom_malloc(numtextures * sizeof(short*)));
    texturecolumnofs = static_cast<unsigned short**>(
        doom_malloc(numtextures * sizeof(unsigned short*)));
    texturecomposite = static_cast<byte**>(doom_malloc(numtextures * sizeof(byte*)));

    texturecompositesize = static_cast<int*>(doom_malloc(numtextures * sizeof(int)));

    texturewidthmask = static_cast<int*>(doom_malloc(numtextures * sizeof(int)));
    textureheight =
        static_cast<fixed_t*>(doom_malloc(numtextures * sizeof(fixed_t)));

    // Really complex printing shit...
    temp1 = W_GetNumForName("S_START"); // P_???????
    temp2 = W_GetNumForName("S_END") - 1;
    temp3 = ((temp2 - temp1 + 63) / 64) + ((numtextures + 63) / 64);
    doom_print("[");
    for (i = 0; i < temp3; i++)
        doom_print(" ");
    doom_print("         ]");
    for (i = 0; i < temp3; i++)
        doom_print("\x8");
    doom_print("\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8");

    for (i = 0; i < numtextures; i++, directory++)
    {
        if (!(i & 63))
            doom_print(".");

        if (i == numtextures1)
        {
            // Start looking in second texture file.
            maptex = maptex2;
            maxoff = maxoff2;
            directory = maptex + 1;
        }

        offset = LONG(*directory);

        if (offset > maxoff)
            I_Error("Error: initTextures: bad texture directory");

        mtexture = reinterpret_cast<maptexture_t*>(reinterpret_cast<byte*>(maptex)
                                                   + offset);

        texture = textures[i] = static_cast<texture_t*>(
            doom_malloc(sizeof(texture_t)
                        + sizeof(texpatch_t) * (SHORT(mtexture->patchcount) - 1)));

        texture->width = SHORT(mtexture->width);
        texture->height = SHORT(mtexture->height);
        texture->patchcount = SHORT(mtexture->patchcount);

        doom_memcpy(texture->name, mtexture->name, sizeof(texture->name));
        mpatch = &mtexture->patches[0];
        patch = &texture->patches[0];

        for (j = 0; j < texture->patchcount; j++, mpatch++, patch++)
        {
            patch->originx = SHORT(mpatch->originx);
            patch->originy = SHORT(mpatch->originy);
            patch->patch = patchlookup[SHORT(mpatch->patch)];
            if (patch->patch == -1)
            {
                //I_Error("Error: initTextures: Missing patch in texture %s",
                //        texture->name);

                doom_strcpy(error_buf,
                            "Error: initTextures: Missing patch in texture ");
                doom_concat(error_buf, texture->name);
                I_Error(error_buf);
            }
        }
        texturecolumnlump[i] =
            static_cast<short*>(doom_malloc(texture->width * sizeof(short)));
        texturecolumnofs[i] = static_cast<unsigned short*>(
            doom_malloc(texture->width * sizeof(unsigned short)));

        j = 1;
        while (j * 2 <= texture->width)
            j <<= 1;

        texturewidthmask[i] = j - 1;
        textureheight[i] = texture->height << FRACBITS;
    }

    // Precalculate whatever possible.
    for (i = 0; i < numtextures; i++)
        generateLookup(i);

    // Create translation table for global animation.
    texturetranslation =
        static_cast<int*>(doom_malloc((numtextures + 1) * sizeof(int)));

    for (i = 0; i < numtextures; i++)
        texturetranslation[i] = i;

    doom_free(patchlookup);
}

//
// initFlats
//
void initFlats()
{
    firstflat = W_GetNumForName("F_START") + 1;
    lastflat = W_GetNumForName("F_END") - 1;
    numflats = lastflat - firstflat + 1;

    // Create translation table for global animation.
    flattranslation = static_cast<int*>(doom_malloc((numflats + 1) * sizeof(int)));

    for (int i = 0; i < numflats; i++)
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
    patch_t* patch;

    firstspritelump = W_GetNumForName("S_START") + 1;
    lastspritelump = W_GetNumForName("S_END") - 1;

    numspritelumps = lastspritelump - firstspritelump + 1;
    spritewidth =
        static_cast<fixed_t*>(doom_malloc(numspritelumps * sizeof(fixed_t)));
    spriteoffset =
        static_cast<fixed_t*>(doom_malloc(numspritelumps * sizeof(fixed_t)));
    spritetopoffset =
        static_cast<fixed_t*>(doom_malloc(numspritelumps * sizeof(fixed_t)));

    for (int i = 0; i < numspritelumps; i++)
    {
        if (!(i & 63))
            doom_print(".");

        patch = static_cast<patch_t*>(W_CacheLumpNum(firstspritelump + i, PU_CACHE));
        spritewidth[i] = SHORT(patch->width) << FRACBITS;
        spriteoffset[i] = SHORT(patch->leftoffset) << FRACBITS;
        spritetopoffset[i] = SHORT(patch->topoffset) << FRACBITS;
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
    lump = W_GetNumForName("COLORMAP");
    length = W_LumpLength(lump) + 255;
    colormaps = static_cast<lighttable_t*>(doom_malloc(length));
    colormaps = (byte*) (((unsigned long long) colormaps + 255) & ~0xff);
    W_ReadLump(lump, colormaps);
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
    doom_print("\nInitTextures");
    initFlats();
    doom_print("\nInitFlats");
    initSpriteLumps();
    doom_print("\nInitSprites");
    initColormaps();
    doom_print("\nInitColormaps");
}

//
// flatNumForName
// Retrieval, get a flat number for a flat name.
//
int flatNumForName(const char* name)
{
    int i;
    char namet[9];

    i = W_CheckNumForName(name);

    if (i == -1)
    {
        namet[8] = 0;
        doom_memcpy(namet, name, 8);
        //I_Error("Error: flatNumForName: %s not found", namet);

        doom_strcpy(error_buf, "Error: flatNumForName: ");
        doom_concat(error_buf, namet);
        doom_concat(error_buf, " not found");
        I_Error(error_buf);
    }
    return i - firstflat;
}

//
// checkTextureNumForName
// Check whether texture is available.
// Filter out NoTexture indicator.
//
int checkTextureNumForName(const char* name)
{
    // "NoTexture" marker.
    if (name[0] == '-')
        return 0;

    for (int i = 0; i < numtextures; i++)
        if (!doom_strncasecmp(textures[i]->name, name, 8))
            return i;

    return -1;
}

//
// textureNumForName
// Calls checkTextureNumForName,
//  aborts with error message.
//
int textureNumForName(const char* name)
{
    int i = checkTextureNumForName(name);

    if (i == -1)
    {
        //I_Error("Error: textureNumForName: %s not found",
        //        name);

        doom_strcpy(error_buf, "Error: textureNumForName: ");
        doom_concat(error_buf, name);
        doom_concat(error_buf, " not found");
        I_Error(error_buf);
    }
    return i;
}

//
// precacheLevel
// Preloads all relevant graphics for the level.
//
void precacheLevel()
{
    char* flatpresent;
    char* texturepresent;
    char* spritepresent;

    int i;
    int j;
    int k;
    int lump;

    texture_t* texture;
    thinker_t* th;
    spriteframe_t* sf;

    if (demoplayback)
        return;

    // Precache flats.
    flatpresent = static_cast<char*>(doom_malloc(numflats));
    doom_memset(flatpresent, 0, numflats);

    for (i = 0; i < numsectors; i++)
    {
        flatpresent[sectors[i].floorpic] = 1;
        flatpresent[sectors[i].ceilingpic] = 1;
    }

    flatmemory = 0;

    for (i = 0; i < numflats; i++)
    {
        if (flatpresent[i])
        {
            lump = firstflat + i;
            flatmemory += lumpinfo[lump].size;
            W_CacheLumpNum(lump, PU_CACHE);
        }
    }

    // Precache textures.
    texturepresent = static_cast<char*>(doom_malloc(numtextures));
    doom_memset(texturepresent, 0, numtextures);

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
    texturepresent[skytexture] = 1;

    texturememory = 0;
    for (i = 0; i < numtextures; i++)
    {
        if (!texturepresent[i])
            continue;

        texture = textures[i];

        for (j = 0; j < texture->patchcount; j++)
        {
            lump = texture->patches[j].patch;
            texturememory += lumpinfo[lump].size;
            W_CacheLumpNum(lump, PU_CACHE);
        }
    }

    // Precache sprites.
    spritepresent = static_cast<char*>(doom_malloc(numsprites));
    doom_memset(spritepresent, 0, numsprites);

    for (th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if (th->function.acp1 == (actionf_p1) P_MobjThinker)
            spritepresent[reinterpret_cast<mobj_t*>(th)->sprite] = 1;
    }

    spritememory = 0;
    for (i = 0; i < numsprites; i++)
    {
        if (!spritepresent[i])
            continue;

        for (j = 0; j < sprites[i].numframes; j++)
        {
            sf = &sprites[i].spriteframes[j];
            for (k = 0; k < 8; k++)
            {
                lump = firstspritelump + sf->lump[k];
                spritememory += lumpinfo[lump].size;
                W_CacheLumpNum(lump, PU_CACHE);
            }
        }
    }

    doom_free(texturepresent);
    doom_free(flatpresent);
    doom_free(spritepresent);
}
} // namespace Doom
