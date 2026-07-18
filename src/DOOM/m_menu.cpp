// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        DOOM selection menu, options, episode etc. Sliders and icons.
//        Rewritten in UI/Menu.{h,cpp}; this keeps the vanilla M_ names as shims.
//        The globals other subsystems read (menuactive, screenblocks and the
//        other config-backed values, inhelpscreens, messageToPrint) are defined
//        at file scope in Menu.cpp, so there is nothing to own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "d_event.h"
#include "m_menu.h"

#include "UI/Menu.h"

doom_boolean M_Responder(event_t* ev)
{
    return Doom::M_Responder(ev);
}

void M_Ticker()
{
    Doom::M_Ticker();
}

void M_Drawer()
{
    Doom::M_Drawer();
}

void M_Init()
{
    Doom::M_Init();
}

void M_StartControlPanel()
{
    Doom::M_StartControlPanel();
}
