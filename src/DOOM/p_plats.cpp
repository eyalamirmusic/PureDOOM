// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Elevator platforms. Rewritten in Sim/Plats.{h,cpp}; this keeps the vanilla
//        names as shims and owns the activeplats storage (p_saveg reads it, and
//        T_PlatRaise stays global for the same reason).
//
//-----------------------------------------------------------------------------

#include "p_local.h"

#include "Sim/ActiveSpecials.h"
#include "Sim/Plats.h"

// activeplats is declared in p_spec.h and read by p_saveg; it is a member of the
// Doom::ActiveSpecials owned by the Engine now, and this vanilla name a reference onto it.
plat_t* (&activeplats)[MAXPLATS] = Doom::activeSpecials().activeplats;

void T_PlatRaise(plat_t* plat)
{
    Doom::platRaise(plat);
}

int EV_DoPlat(line_t* line, plattype_e type, int amount)
{
    return Doom::doPlat(line, type, amount);
}

void P_ActivateInStasis(int tag)
{
    Doom::activateInStasis(tag);
}

void EV_StopPlat(line_t* line)
{
    Doom::stopPlat(line);
}

void P_AddActivePlat(plat_t* plat)
{
    Doom::addActivePlat(plat);
}

void P_RemoveActivePlat(plat_t* plat)
{
    Doom::removeActivePlat(plat);
}
