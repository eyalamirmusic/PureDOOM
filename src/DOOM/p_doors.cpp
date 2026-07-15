// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Vertical doors. Rewritten in Sim/Doors.{h,cpp}; this keeps the vanilla
//        names as shims. T_VerticalDoor stays global for the p_saveg-identity
//        reason.
//
//-----------------------------------------------------------------------------

#include "p_local.h"

#include "Sim/Doors.h"

void T_VerticalDoor(vldoor_t* door)
{
    Doom::verticalDoor(door);
}

int EV_DoLockedDoor(line_t* line, vldoor_e type, mobj_t* thing)
{
    return Doom::doLockedDoor(line, type, thing);
}

int EV_DoDoor(line_t* line, vldoor_e type)
{
    return Doom::doDoor(line, type);
}

void EV_VerticalDoor(line_t* line, mobj_t* thing)
{
    Doom::verticalDoor(line, thing);
}

void P_SpawnDoorCloseIn30(sector_t* sec)
{
    Doom::spawnDoorCloseIn30(sec);
}

void P_SpawnDoorRaiseIn5Mins(sector_t* sec, int secnum)
{
    Doom::spawnDoorRaiseIn5Mins(sec, secnum);
}
