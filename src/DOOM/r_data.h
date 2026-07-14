// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//  Refresh module, data I/O, caching, retrieval of graphics
//  by name.
//
//-----------------------------------------------------------------------------

#ifndef __R_DATA__
#define __R_DATA__


#include "r_defs.h"
#include "r_state.h"


// Retrieve column data for span blitting.
// A single patch from a texture definition,
// basically a rectangular area within
// the texture rectangle.
typedef struct
{
    // Block origin (allways UL),
    // which has allready accounted
    // for the internal origin of the patch.
    int originx;
    int originy;
    int patch;
} texpatch_t;


// A maptexturedef_t describes a rectangular texture,
// which is composed of one or more mappatch_t structures
// that arrange graphic patches.
typedef struct
{
    // Keep name for switch changing, etc.
    char name[8];
    short width;
    short height;

    // All the patches[patchcount]
    //  are drawn back to front into the cached texture.
    short patchcount;
    texpatch_t patches[1];
} texture_t;


// Every wall texture the WAD loaded, and how many.
//
// A texture is a list of patches to be drawn back to front, not a bitmap - which
// is why a masked texture's holes are holes: R_GenerateComposite leaves whatever
// no patch covered. Anything that wants the pixels rather than the engine's
// cached columns (which are post data, not pixels, for exactly those textures)
// has to compose them the same way.
extern texture_t** textures;


byte* R_GetColumn(int tex, int col);

// I/O, setting up the stuff.
void R_InitData(void);
void R_PrecacheLevel(void);

// Retrieval.
// Floor/ceiling opaque texture tiles,
// lookup by name. For animation?
int R_FlatNumForName(char* name);

// Called by P_Ticker for switches and animations,
// returns the texture number for the texture name.
int R_TextureNumForName(char* name);
int R_CheckTextureNumForName(char* name);


// How many wall textures and flats the WAD loaded. Composed lazily, so these are
// the id space anything walking the graphics has to work in.
extern int numtextures;
extern int numflats;


#endif

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
