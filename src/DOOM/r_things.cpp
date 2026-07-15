// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Sprite / thing rendering. Rewritten in Render/Things.{h,cpp}; this keeps
//        the R_ names as shims and owns the vissprite pool and psprite-clip
//        globals the other renderer files share.
//
//-----------------------------------------------------------------------------

#include "r_local.h"

#include "Render/GraphicsData.h"
#include "Render/Things.h"

// Sprite scaling for the player's own weapon sprites (read by r_main/r_plane).
fixed_t pspritescale;
fixed_t pspriteiscale;

// Constant arrays used for psprite clipping and initializing clipping
//  (read by r_segs/r_main).
short negonearray[SCREENWIDTH];
short screenheightarray[SCREENWIDTH];

// Variables used to look up and range check thing_t sprites patches
//  (read across the renderer and the app). The sprite frame table lives in
//  Doom::GraphicsData (an Engine member) now; these are references onto it.
spritedef_t*& sprites = Doom::graphicsData().sprites;
int& numsprites = Doom::graphicsData().numsprites;

// The vissprite pool and its sorted list head (read by r_segs).
vissprite_t vissprites[MAXVISSPRITES];
vissprite_t* vissprite_p;
vissprite_t vsprsortedhead;

// The masked-column clip windows and sprite scale (read by r_segs).
short* mfloorclip;
short* mceilingclip;
fixed_t spryscale;
fixed_t sprtopscreen;


void R_DrawMaskedColumn(column_t* column)
{
    Doom::drawMaskedColumn(column);
}

void R_SortVisSprites(void)
{
    Doom::sortVisSprites();
}

void R_AddSprites(sector_t* sec)
{
    Doom::addSprites(sec);
}

void R_InitSprites(char** namelist)
{
    Doom::initSprites(namelist);
}

void R_ClearSprites(void)
{
    Doom::clearSprites();
}

void R_DrawMasked(void)
{
    Doom::drawMasked();
}
