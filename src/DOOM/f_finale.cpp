// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Game completion, final screen animation. Rewritten in UI/Finale.{h,cpp};
//        this keeps the F_ names as shims. All finale state is file-local to the
//        rewritten unit, so there are no globals to own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "f_finale.h"

#include "UI/Finale.h"

doom_boolean F_Responder(event_t* ev)
{
    return Doom::fResponder(ev);
}

void F_Ticker(void)
{
    Doom::fTicker();
}

void F_Drawer(void)
{
    Doom::fDrawer();
}

void F_StartFinale(void)
{
    Doom::fStartFinale();
}
