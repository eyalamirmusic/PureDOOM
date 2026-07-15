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
//        Moving object handling. Rewritten in Sim/Mobj.{h,cpp}; this keeps the
//        vanilla names as shims. P_MobjThinker keeps its global address because
//        p_saveg and the sim probe identify mobjs by comparing to it.
//
//-----------------------------------------------------------------------------

#include "doomdef.h"
#include "p_local.h"

#include "Sim/ItemRespawnQueue.h"
#include "Sim/Mobj.h"


// The item-respawn queue. iquehead/iquetail are reset by p_setup on level load; the
// arrays are read only here and by Sim/Mobj.cpp through these definitions.
// The item respawn queue is a Doom::ItemRespawnQueue owned by the Engine now; these are
// references onto it, the arrays as references-to-array (REFACTOR.md, Step 5).
mapthing_t (&itemrespawnque)[ITEMQUESIZE] = Doom::itemRespawnQueue().itemrespawnque;
int (&itemrespawntime)[ITEMQUESIZE] = Doom::itemRespawnQueue().itemrespawntime;
int& iquehead = Doom::itemRespawnQueue().iquehead;
int& iquetail = Doom::itemRespawnQueue().iquetail;


doom_boolean P_SetMobjState(mobj_t* mobj, statenum_t state)
{
    return Doom::setMobjState(mobj, state);
}

void P_MobjThinker(mobj_t* mobj)
{
    Doom::mobjThinker(mobj);
}

mobj_t* P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type)
{
    return Doom::spawnMobj(x, y, z, type);
}

void P_RemoveMobj(mobj_t* mobj)
{
    Doom::removeMobj(mobj);
}

void P_RespawnSpecials(void)
{
    Doom::respawnSpecials();
}

void P_SpawnPlayer(mapthing_t* mthing)
{
    Doom::spawnPlayer(mthing);
}

void P_SpawnMapThing(mapthing_t* mthing)
{
    Doom::spawnMapThing(mthing);
}

void P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z)
{
    Doom::spawnPuff(x, y, z);
}

void P_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, int damage)
{
    Doom::spawnBlood(x, y, z, damage);
}

mobj_t* P_SpawnMissile(mobj_t* source, mobj_t* dest, mobjtype_t type)
{
    return Doom::spawnMissile(source, dest, type);
}

void P_SpawnPlayerMissile(mobj_t* source, mobjtype_t type)
{
    Doom::spawnPlayerMissile(source, type);
}
