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
//
//-----------------------------------------------------------------------------

#include "../Host/Platform.h"

#include "Info.h" // We are referring to sprite numbers.
#include "ItemTypes.h"

// clang-format off

// The weapon table below is kept verbatim from the 1993 source, so it names
// am_*/S_* unqualified. Those enums live in namespace Doom now; this brings
// them into scope without touching the table.
using namespace Doom;


//
// PSPRITE ACTIONS for waepons.
// This struct controls the weapon animations.
//
// Each entry is:
//   ammo/amunition type
//  upstate
//  downstate
// readystate
// atkstate, i.e. attack/fire/hit frame
// flashstate, muzzle flash
//
WeaponInfo weaponinfo[numWeapons] =
{
    {
        // fist
        AmmoType::NoAmmo,
        StateNum::Punchup,
        StateNum::Punchdown,
        StateNum::Punch,
        StateNum::Punch1,
        StateNum::Null
    },
    {
        // pistol
        AmmoType::Clip,
        StateNum::Pistolup,
        StateNum::Pistoldown,
        StateNum::Pistol,
        StateNum::Pistol1,
        StateNum::Pistolflash
    },
    {
        // shotgun
        AmmoType::Shell,
        StateNum::Sgunup,
        StateNum::Sgundown,
        StateNum::Sgun,
        StateNum::Sgun1,
        StateNum::Sgunflash1
    },
    {
        // chaingun
        AmmoType::Clip,
        StateNum::Chainup,
        StateNum::Chaindown,
        StateNum::Chain,
        StateNum::Chain1,
        StateNum::Chainflash1
    },
    {
        // missile launcher
        AmmoType::Misl,
        StateNum::Missileup,
        StateNum::Missiledown,
        StateNum::Missile,
        StateNum::Missile1,
        StateNum::Missileflash1
    },
    {
        // plasma rifle
        AmmoType::Cell,
        StateNum::Plasmaup,
        StateNum::Plasmadown,
        StateNum::Plasma,
        StateNum::Plasma1,
        StateNum::Plasmaflash1
    },
    {
        // bfg 9000
        AmmoType::Cell,
        StateNum::Bfgup,
        StateNum::Bfgdown,
        StateNum::Bfg,
        StateNum::Bfg1,
        StateNum::Bfgflash1
    },
    {
        // chainsaw
        AmmoType::NoAmmo,
        StateNum::Sawup,
        StateNum::Sawdown,
        StateNum::Saw,
        StateNum::Saw1,
        StateNum::Null
    },
    {
        // super shotgun
        AmmoType::Shell,
        StateNum::Dsgunup,
        StateNum::Dsgundown,
        StateNum::Dsgun,
        StateNum::Dsgun1,
        StateNum::Dsgunflash1
    },
};

// clang-format on
