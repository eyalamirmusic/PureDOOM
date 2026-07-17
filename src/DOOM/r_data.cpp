// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Renderer texture/flat/sprite/colormap data. Rewritten in
//        Render/Data.{h,cpp}; this keeps the R_ names as shims and owns the
//        externally-read table globals.
//
//-----------------------------------------------------------------------------

#include "r_local.h"

#include "Render/Data.h"
#include "Render/GraphicsData.h"

// The renderer's loaded graphics data (textures, flats, sprite lumps, colormaps) is a
// Doom::GraphicsData owned by the Engine now; these vanilla names are references onto it.
// R_InitData fills the members once at startup; they are read-only after.
int& firstflat = Doom::graphicsData().firstflat;
int& numflats = Doom::graphicsData().numflats;


int& firstspritelump = Doom::graphicsData().firstspritelump;
int& lastspritelump = Doom::graphicsData().lastspritelump;
int& numspritelumps = Doom::graphicsData().numspritelumps;

int& numtextures = Doom::graphicsData().numtextures;
texture_t**& textures = Doom::graphicsData().textures;

// needed for texture pegging
fixed_t*& textureheight = Doom::graphicsData().textureheight;

// for global animation
int*& flattranslation = Doom::graphicsData().flattranslation;
int*& texturetranslation = Doom::graphicsData().texturetranslation;

// needed for pre rendering. Plain-pointer views onto GraphicsData's owned EA::Vectors
// (Step 9); initSpriteLumps points them at data() after filling the vectors.
fixed_t* spritewidth = nullptr;
fixed_t* spriteoffset = nullptr;
fixed_t* spritetopoffset = nullptr;

lighttable_t*& colormaps = Doom::graphicsData().colormaps;


void R_DrawColumnInCache(column_t* patch, byte* cache, int originy, int cacheheight)
{
    Doom::drawColumnInCache(patch, cache, originy, cacheheight);
}

void R_GenerateComposite(int texnum)
{
    Doom::generateComposite(texnum);
}

void R_GenerateLookup(int texnum)
{
    Doom::generateLookup(texnum);
}

byte* R_GetColumn(int tex, int col)
{
    return Doom::getColumn(tex, col);
}

void R_InitTextures(void)
{
    Doom::initTextures();
}

void R_InitFlats(void)
{
    Doom::initFlats();
}

void R_InitSpriteLumps(void)
{
    Doom::initSpriteLumps();
}

void R_InitColormaps(void)
{
    Doom::initColormaps();
}

void R_InitData(void)
{
    Doom::initData();
}

int R_FlatNumForName(const char* name)
{
    return Doom::flatNumForName(name);
}

int R_CheckTextureNumForName(const char* name)
{
    return Doom::checkTextureNumForName(name);
}

int R_TextureNumForName(const char* name)
{
    return Doom::textureNumForName(name);
}

void R_PrecacheLevel(void)
{
    Doom::precacheLevel();
}
