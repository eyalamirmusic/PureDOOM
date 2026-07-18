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
lighttable_t* colormaps = nullptr;














