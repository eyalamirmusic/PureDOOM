// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Thinker list and ticker. Rewritten in Sim/Tick.{h,cpp}; this keeps the
//        vanilla names as shims and owns leveltime and thinkercap.
//
//-----------------------------------------------------------------------------

#include "p_local.h"

#include "Sim/Tick.h"

// leveltime (the level clock) and thinkercap (the thinker list head) are read
// across the engine; their storage lives here.
int leveltime;
thinker_t thinkercap;

void P_InitThinkers(void)
{
    Doom::initThinkers();
}

void P_AddThinker(thinker_t* thinker)
{
    Doom::addThinker(thinker);
}

void P_RemoveThinker(thinker_t* thinker)
{
    Doom::removeThinker(thinker);
}

void P_RunThinkers(void)
{
    Doom::runThinkers();
}

void P_Ticker(void)
{
    Doom::ticker();
}
