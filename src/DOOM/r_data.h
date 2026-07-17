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

#pragma once


#include "r_defs.h"
#include "r_state.h"

// texture_t owns its patches in an EA::Vector now (RAII, REFACTOR.md Step 9).
#include <ea_data_structures/Structures/Vector.h>


// Retrieve column data for span blitting.
// A single patch from a texture definition,
// basically a rectangular area within
// the texture rectangle.
struct texpatch_t
{
    // Block origin (allways UL),
    // which has allready accounted
    // for the internal origin of the patch.
    int originx;
    int originy;
    int patch;
};


// A maptexturedef_t describes a rectangular texture,
// which is composed of one or more mappatch_t structures
// that arrange graphic patches.
struct texture_t
{
    // Keep name for switch changing, etc.
    char name[8];
    short width;
    short height;

    // All the patches[patchcount]
    //  are drawn back to front into the cached texture. RAII-owned (Step 9): was a
    //  trailing flexible-array-member (patches[1] with a variable-length malloc);
    //  now an owned vector, so a texture_t is fixed-size and frees its own patches.
    //  Readers index it as before (patches[j], &patches[0]).
    short patchcount;
    EA::Vector<texpatch_t> patches;
};


// Every wall texture the WAD loaded, and how many.
//
// A texture is a list of patches to be drawn back to front, not a bitmap - which
// is why a masked texture's holes are holes: R_GenerateComposite leaves whatever
// no patch covered. Anything that wants the pixels rather than the engine's
// cached columns (which are post data, not pixels, for exactly those textures)
// has to compose them the same way.
// The texture table lives in Doom::GraphicsData (an Engine member) now. It owns the
// texture_t structs by value; `textures` stays a texture_t** (a view onto a pointer
// array into that storage) so every `textures[i]->field` reader is unchanged (Step 9).
extern texture_t** textures;


byte* R_GetColumn(int tex, int col);

// I/O, setting up the stuff.
void R_InitData(void);
void R_PrecacheLevel(void);

// Retrieval.
// Floor/ceiling opaque texture tiles,
// lookup by name. For animation?
int R_FlatNumForName(const char* name);

// Called by P_Ticker for switches and animations,
// returns the texture number for the texture name.
int R_TextureNumForName(const char* name);
int R_CheckTextureNumForName(const char* name);


// How many wall textures and flats the WAD loaded. Composed lazily, so these are
// the id space anything walking the graphics has to work in.
extern int& numtextures;
extern int& numflats;



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
