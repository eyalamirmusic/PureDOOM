// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//        Handling interactions (i.e., collisions).
//
// The logic is rewritten in Sim/Interaction.{h,cpp}; this keeps the vanilla names
// as shims and the ammo data tables (maxammo is read by st_stuff and g_game).
//
//-----------------------------------------------------------------------------

#include "doomdef.h"
#include "p_local.h"

#include "Game/AmmoLimits.h"
#include "Sim/Interaction.h"

#ifdef __GNUG__
#pragma implementation "p_inter.h"
#endif
#include "p_inter.h"


// a weapon is found with two clip loads, a big item has five clip loads
// maxammo is a Doom::AmmoLimits owned by the Engine now; this is a reference-to-array onto
// it (REFACTOR.md, Step 5).
int (&maxammo)[NUMAMMO] = Doom::ammoLimits().maxammo;
int clipammo[NUMAMMO] = {10, 4, 20, 1};


void P_TouchSpecialThing(mobj_t* special, mobj_t* toucher)
{
    Doom::touchSpecialThing(special, toucher);
}


void P_DamageMobj(mobj_t* target, mobj_t* inflictor, mobj_t* source, int damage)
{
    Doom::damageMobj(target, inflictor, source, damage);
}


doom_boolean P_GivePower(player_t* player, int power)
{
    return Doom::givePower(player, power);
}
