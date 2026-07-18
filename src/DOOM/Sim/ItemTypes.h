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
// DESCRIPTION:
//        Items: key cards, artifacts, weapon, ammunition.
//
//-----------------------------------------------------------------------------

#pragma once


#include "../Game/GameDefs.h"


// Weapon info: sprite frames, ammunition use.
namespace Doom
{
struct WeaponInfo
{
    Doom::AmmoType ammo;
    int upstate;
    int downstate;
    int readystate;
    int atkstate;
    int flashstate;
};
} // namespace Doom


extern Doom::WeaponInfo weaponinfo[Doom::NUMWEAPONS];



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
