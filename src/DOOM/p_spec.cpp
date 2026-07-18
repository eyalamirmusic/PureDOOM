// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Special effects coordinator. Rewritten in Sim/Specials.{h,cpp}; this keeps
//        the vanilla names as shims and owns levelTimer/levelTimeCount.
//
//-----------------------------------------------------------------------------

#include "p_local.h"

#include "Sim/EndLevelTimer.h"
#include "Sim/Specials.h"

// levelTimer/levelTimeCount are declared in p_spec.h (the -TIMER option); they are members of
// the Doom::EndLevelTimer owned by the Engine now, and these vanilla names references onto it.
doom_boolean& levelTimer = Doom::endLevelTimer().levelTimer;
int& levelTimeCount = Doom::endLevelTimer().levelTimeCount;

void P_InitPicAnims()
{
    Doom::initPicAnims();
}

fixed_t P_FindLowestFloorSurrounding(sector_t* sec)
{
    return Doom::findLowestFloorSurrounding(sec);
}

fixed_t P_FindHighestFloorSurrounding(sector_t* sec)
{
    return Doom::findHighestFloorSurrounding(sec);
}

fixed_t P_FindNextHighestFloor(sector_t* sec, int currentheight)
{
    return Doom::findNextHighestFloor(sec, currentheight);
}

fixed_t P_FindLowestCeilingSurrounding(sector_t* sec)
{
    return Doom::findLowestCeilingSurrounding(sec);
}

fixed_t P_FindHighestCeilingSurrounding(sector_t* sec)
{
    return Doom::findHighestCeilingSurrounding(sec);
}

int P_FindSectorFromLineTag(line_t* line, int start)
{
    return Doom::findSectorFromLineTag(line, start);
}

int P_FindMinSurroundingLight(sector_t* sector, int max)
{
    return Doom::findMinSurroundingLight(sector, max);
}

void P_CrossSpecialLine(int linenum, int side, mobj_t* thing)
{
    Doom::crossSpecialLine(linenum, side, thing);
}

void P_ShootSpecialLine(mobj_t* thing, line_t* line)
{
    Doom::shootSpecialLine(thing, line);
}

void P_PlayerInSpecialSector(player_t* player)
{
    Doom::playerInSpecialSector(player);
}

void P_UpdateSpecials()
{
    Doom::updateSpecials();
}

int EV_DoDonut(line_t* line)
{
    return Doom::doDonut(line);
}

void P_SpawnSpecials()
{
    Doom::spawnSpecials();
}
