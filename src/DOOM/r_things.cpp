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
#include "Render/SpriteState.h"
#include "Render/Things.h"

// The sprite-drawing state r_things exports is a Doom::SpriteState owned by the Engine now; the
// definitions below are references onto its members (REFACTOR.md, Step 5).

// Sprite scaling for the player's own weapon sprites (read by r_main/r_plane).
fixed_t& pspritescale = Doom::spriteState().pspritescale;
fixed_t& pspriteiscale = Doom::spriteState().pspriteiscale;

// Constant arrays used for psprite clipping and initializing clipping
//  (read by r_segs/r_main).
short (&negonearray)[SCREENWIDTH] = Doom::spriteState().negonearray;
short (&screenheightarray)[SCREENWIDTH] = Doom::spriteState().screenheightarray;

// Variables used to look up and range check thing_t sprites patches
//  (read across the renderer and the app). The sprite frame table lives in
//  Doom::GraphicsData (an Engine member) now; numsprites is a reference onto it,
//  and sprites is a plain-pointer view onto its owned EA::Vector, set by
//  R_InitSpriteDefs after the fill (Step 9).
spritedef_t* sprites = nullptr;
int& numsprites = Doom::graphicsData().numsprites;

// The vissprite pool and its sorted list head (read by r_segs).
vissprite_t (&vissprites)[MAXVISSPRITES] = Doom::spriteState().vissprites;
vissprite_t*& vissprite_p = Doom::spriteState().vissprite_p;
vissprite_t& vsprsortedhead = Doom::spriteState().vsprsortedhead;

// The masked-column clip windows and sprite scale (read by r_segs).
short*& mfloorclip = Doom::spriteState().mfloorclip;
short*& mceilingclip = Doom::spriteState().mceilingclip;
fixed_t& spryscale = Doom::spriteState().spryscale;
fixed_t& sprtopscreen = Doom::spriteState().sprtopscreen;







