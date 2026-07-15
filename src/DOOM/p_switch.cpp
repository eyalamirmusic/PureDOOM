// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Switches, buttons. Rewritten in Sim/Switches.{h,cpp}; this keeps the
//        vanilla names as shims and owns the buttonlist storage (p_spec ticks it).
//
//-----------------------------------------------------------------------------

#include "p_local.h"

#include "Sim/Switches.h"

// buttonlist is declared in p_spec.h and ticked by p_spec's P_UpdateSpecials.
button_t buttonlist[MAXBUTTONS];

void P_InitSwitchList(void)
{
    Doom::initSwitchList();
}

void P_StartButton(line_t* line, bwhere_e w, int texture, int time)
{
    Doom::startButton(line, w, texture, time);
}

void P_ChangeSwitchTexture(line_t* line, int useAgain)
{
    Doom::changeSwitchTexture(line, useAgain);
}

doom_boolean P_UseSpecialLine(mobj_t* thing, line_t* line, int side)
{
    return Doom::useSpecialLine(thing, line, side);
}
