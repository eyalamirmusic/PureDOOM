// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Mission begin melt/wipe screen special effect. Rewritten in
//        UI/Wipe.{h,cpp}; this keeps the wipe_ names as shims and owns the melt
//        state a GPU compositor reads.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "doomtype.h" // doom_boolean, byte (the melt-state globals' types)
#include "f_wipe.h"

#include "UI/Wipe.h"

//
// SCREEN WIPE PACKAGE
//

// Raised while a melt is running, and the only safe thing to test: wipe_exitMelt
// frees the column table without clearing the pointer to it. (Read by the GPU
// melt compositor in EngineAccess.)
doom_boolean wipe_melt_running = 0;

// The outgoing frame, as palette indices; wipe_initMelt leaves it column-major.
byte* wipe_scr_start;

// How far down each two-pixel column of the outgoing screen has slid so far.
// Negative means the column has not started moving yet.
int* wipe_melt_offsets;




