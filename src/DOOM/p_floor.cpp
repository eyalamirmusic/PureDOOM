// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Floor movement / stairs. Rewritten in Sim/Floors.{h,cpp}; this keeps the
//        vanilla names as shims. T_MovePlane stays global because the other
//        specials call it; T_MoveFloor was deleted once ThinkerDispatch.cpp started
//        calling Doom::moveFloor directly.
//
//-----------------------------------------------------------------------------

#include "p_local.h"

#include "Sim/Floors.h"

Doom::MoveResult T_MovePlane(Doom::Sector* sector, fixed_t speed, fixed_t dest, doom_boolean crush, int floorOrCeiling, int direction)
{
    return Doom::movePlane(*sector, speed, dest, crush, floorOrCeiling, direction);
}

