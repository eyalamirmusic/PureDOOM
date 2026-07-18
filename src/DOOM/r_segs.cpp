// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Wall/seg rendering. Rewritten in Render/Segs.{h,cpp}; this keeps the R_
//        names as shims and owns the wall-column globals.
//
//-----------------------------------------------------------------------------

#include "r_local.h"

#include "Render/RenderScratch.h"
#include "Render/SegState.h"
#include "Render/Segs.h"

#define HEIGHTBITS 12
#define HEIGHTUNIT (1<<HEIGHTBITS)

// The wall-segment state r_segs exports (segtextured, the mark flags, the chosen textures, the
// column range, the light row and the masked-column list) is a Doom::SegState owned by the Engine
// now; these are references onto its members (REFACTOR.md, Step 5).
doom_boolean& segtextured = Doom::segState().segtextured;


// False if the back side is the same plane.
doom_boolean& markfloor = Doom::segState().markfloor;

doom_boolean& markceiling = Doom::segState().markceiling;

int& toptexture = Doom::segState().toptexture;

int& bottomtexture = Doom::segState().bottomtexture;

int& midtexture = Doom::segState().midtexture;


// The current wall segment's projection lives in Doom::RenderScratch (an Engine member)
// now; these are references onto it.
angle_t& rw_normalangle = Doom::renderScratch().rw_normalangle;

// angle to line origin
int& rw_angle1 = Doom::renderScratch().rw_angle1;


//
// regular wall
//
int& rw_x = Doom::segState().rw_x;

int& rw_stopx = Doom::segState().rw_stopx;

fixed_t& rw_distance = Doom::renderScratch().rw_distance;


lighttable_t**& walllights = Doom::segState().walllights;


short*& maskedtexturecol = Doom::segState().maskedtexturecol;



//
// R_RenderMaskedSegRange
//



