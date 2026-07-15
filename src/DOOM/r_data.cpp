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

int firstflat;
int numflats;


int firstspritelump;
int lastspritelump;
int numspritelumps;

int numtextures;
texture_t** textures;

// needed for texture pegging
fixed_t* textureheight;

// for global animation
int* flattranslation;
int* texturetranslation;

// needed for pre rendering
fixed_t* spritewidth;
fixed_t* spriteoffset;
fixed_t* spritetopoffset;

lighttable_t* colormaps;


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

int R_FlatNumForName(char* name)
{
    return Doom::flatNumForName(name);
}

int R_CheckTextureNumForName(char* name)
{
    return Doom::checkTextureNumForName(name);
}

int R_TextureNumForName(char* name)
{
    return Doom::textureNumForName(name);
}

void R_PrecacheLevel(void)
{
    Doom::precacheLevel();
}
