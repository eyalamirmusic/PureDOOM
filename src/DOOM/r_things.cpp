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


#include "Render/GraphicsData.h"
#include "Render/SpriteState.h"
#include "Render/Things.h"

// The sprite-drawing state r_things exports is a Doom::SpriteState owned by the Engine now; the
// definitions below are references onto its members (REFACTOR.md, Step 5).

// Sprite scaling for the player's own weapon sprites (read by r_main/r_plane).

// Constant arrays used for psprite clipping and initializing clipping
//  (read by r_segs/r_main).

// Variables used to look up and range check thing_t sprites patches
//  (read across the renderer and the app). The sprite frame table lives in
//  Doom::GraphicsData (an Engine member) now; numsprites is a reference onto it,
//  and sprites is a plain-pointer view onto its owned EA::Vector, set by
//  R_InitSpriteDefs after the fill (Step 9).
Doom::SpriteDef* sprites = nullptr;

// The vissprite pool and its sorted list head (read by r_segs).

// The masked-column clip windows and sprite scale (read by r_segs).







