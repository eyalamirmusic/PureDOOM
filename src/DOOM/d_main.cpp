// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        DOOM main program (D_DoomMain) and game loop (D_DoomLoop), plus the
//        game-mode detection, argument parsing and startup. Rewritten in
//        Game/DoomMain.{h,cpp}; this keeps the D_ names as shims. The core state
//        d_main owns is defined at file scope in DoomMain.cpp (above its
//        namespace), so there is nothing to own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "d_event.h"
#include "d_main.h"

#include "Game/DoomMain.h"

void D_PostEvent(event_t* ev)
{
    Doom::dPostEvent(ev);
}

void D_ProcessEvents()
{
    Doom::dProcessEvents();
}

void D_Display()
{
    Doom::dDisplay();
}

void D_UpdateWipe()
{
    Doom::dUpdateWipe();
}

void D_DoomLoop()
{
    Doom::dDoomLoop();
}

void D_PageTicker()
{
    Doom::dPageTicker();
}

void D_PageDrawer()
{
    Doom::dPageDrawer();
}

void D_AdvanceDemo()
{
    Doom::dAdvanceDemo();
}

void D_DoAdvanceDemo()
{
    Doom::dDoAdvanceDemo();
}

void D_StartTitle()
{
    Doom::dStartTitle();
}

void D_AddFile(const char* file)
{
    Doom::dAddFile(file);
}

void D_DoomMain()
{
    Doom::dDoomMain();
}
