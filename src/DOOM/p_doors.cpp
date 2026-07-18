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
    Doom::verticalDoor(*door);
}





