// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Moving ceilings/crushers. Rewritten in Sim/Ceilings.{h,cpp}; this keeps the
//        vanilla names as shims and owns the activeceilings storage (p_saveg reads
//        it; T_MoveCeiling stays global for the same reason).
//
//-----------------------------------------------------------------------------

#include "p_local.h"

#include "Sim/Ceilings.h"

// activeceilings is declared in p_spec.h and read by p_saveg; storage lives here.
ceiling_t* activeceilings[MAXCEILINGS];

void T_MoveCeiling(ceiling_t* ceiling)
{
    Doom::moveCeiling(ceiling);
}

int EV_DoCeiling(line_t* line, ceiling_e type)
{
    return Doom::doCeiling(line, type);
}

void P_AddActiveCeiling(ceiling_t* c)
{
    Doom::addActiveCeiling(c);
}

void P_RemoveActiveCeiling(ceiling_t* c)
{
    Doom::removeActiveCeiling(c);
}

void P_ActivateInStasisCeiling(line_t* line)
{
    Doom::activateInStasisCeiling(line);
}

int EV_CeilingCrushStop(line_t* line)
{
    return Doom::ceilingCrushStop(line);
}
