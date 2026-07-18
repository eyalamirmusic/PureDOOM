// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Doom::Column / span drawing. Rewritten in Render/Draw.{h,cpp}; this keeps the
//        R_ names as shims (r_main stores their addresses in the drawer pointers)
//        and owns the view geometry and the drawer input state (dc_*, ds_*) the
//        other renderer files fill in.
//
//-----------------------------------------------------------------------------

#include "r_local.h"

#include "Render/Draw.h"
#include "Render/DrawState.h"
#include "Render/ViewWindow.h"

//
// The view window geometry (set by r_main, read across the renderer and app). The
// storage is a Doom::ViewWindow owned by the Engine now; these vanilla names are
// references onto it.
//

//
// The column/span drawer inputs are a Doom::DrawState owned by the Engine now; these are
// references onto its members (REFACTOR.md, Step 5).
//

//
// R_DrawColumn input: the caller (r_segs/r_plane/r_things) fills these in, the
// column drawers in Render/Draw.cpp read them.
//

// first pixel in a column (possibly virtual)

// Translation tables for player-sprite recolouring (read by r_things).
// A 256-byte-aligned view into DrawState's owned translationTableStorage;
// R_InitTranslationTables points it at the aligned offset (Step 9).
byte* translationtables = nullptr;

//
// R_DrawSpan input: r_plane fills these in, the span drawers read them.
//



// start of a 64*64 tile image












