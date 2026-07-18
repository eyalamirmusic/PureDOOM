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
//        Weapon sprite/animation. Rewritten in Sim/Weapon.{h,cpp}; this keeps the
//        P_* names as shims. The A_* actions moved to Sim/Actions.{h,cpp}
//        (Doom::Actions::*), unprefixed, once nothing outside Sim/Info.cpp needed
//        their global names.
//
//-----------------------------------------------------------------------------

#include "d_player.h"
#include "p_mobj.h"
#include "p_pspr.h"

#include "Sim/Weapon.h"


void P_SetupPsprites(player_t* player)
{
    Doom::setupPsprites(*player);
}

void P_MovePsprites(player_t* player)
{
    Doom::movePsprites(*player);
}

void P_DropWeapon(player_t* player)
{
    Doom::dropWeapon(*player);
}
