// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Intermission screens. Rewritten in UI/Intermission.{h,cpp}; this keeps
//        the WI_ names as shims. All intermission state is file-local to the
//        rewritten unit, so there are no globals to own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "d_player.h" // wbstartstruct_t
#include "wi_stuff.h"

#include "UI/Intermission.h"

void WI_Ticker()
{
    Doom::wiTicker();
}

void WI_Drawer()
{
    Doom::wiDrawer();
}

void WI_Start(wbstartstruct_t* wbstartstruct)
{
    Doom::wiStart(wbstartstruct);
}
