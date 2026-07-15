// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Status bar code (widgets, the face indicator, the palette flashes, the
//        cheat responder). Rewritten in UI/StatusBar.{h,cpp}; this keeps the ST_
//        names as shims and owns st_statusbaron, which the app reads to know
//        whether the status-bar strip should be composited.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "doomtype.h" // doom_boolean
#include "st_stuff.h"

#include "UI/StatusBar.h"

// Whether the left-side main status bar is active. The GPU renderer (EngineAccess)
// reads it: ST_Drawer draws no bar when it is false, so the compositor must not
// sample the strip then.
doom_boolean st_statusbaron;


doom_boolean ST_Responder(event_t* ev)
{
    return Doom::stResponder(ev);
}

void ST_Ticker(void)
{
    Doom::stTicker();
}

void ST_Drawer(doom_boolean fullscreen, doom_boolean refresh)
{
    Doom::stDrawer(fullscreen, refresh);
}

void ST_Start(void)
{
    Doom::stStart();
}

void ST_Init(void)
{
    Doom::stInit();
}
