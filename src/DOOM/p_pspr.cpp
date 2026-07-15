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
//        vanilla names as shims. The A_* actions stay global because info.cpp's
//        state table references them by address.
//
//-----------------------------------------------------------------------------

#include "d_player.h"
#include "p_mobj.h"
#include "p_pspr.h"

#include "Sim/Weapon.h"


void A_WeaponReady(player_t* player, pspdef_t* psp)
{
    Doom::weaponReady(player, psp);
}

void A_ReFire(player_t* player, pspdef_t* psp)
{
    Doom::reFire(player, psp);
}

void A_CheckReload(player_t* player, pspdef_t* psp)
{
    Doom::checkReload(player, psp);
}

void A_Lower(player_t* player, pspdef_t* psp)
{
    Doom::lower(player, psp);
}

void A_Raise(player_t* player, pspdef_t* psp)
{
    Doom::raise(player, psp);
}

void A_GunFlash(player_t* player, pspdef_t* psp)
{
    Doom::gunFlash(player, psp);
}

void A_Punch(player_t* player, pspdef_t* psp)
{
    Doom::punch(player, psp);
}

void A_Saw(player_t* player, pspdef_t* psp)
{
    Doom::saw(player, psp);
}

void A_FireMissile(player_t* player, pspdef_t* psp)
{
    Doom::fireMissile(player, psp);
}

void A_FireBFG(player_t* player, pspdef_t* psp)
{
    Doom::fireBFG(player, psp);
}

void A_FirePlasma(player_t* player, pspdef_t* psp)
{
    Doom::firePlasma(player, psp);
}

void A_FirePistol(player_t* player, pspdef_t* psp)
{
    Doom::firePistol(player, psp);
}

void A_FireShotgun(player_t* player, pspdef_t* psp)
{
    Doom::fireShotgun(player, psp);
}

void A_FireShotgun2(player_t* player, pspdef_t* psp)
{
    Doom::fireShotgun2(player, psp);
}

void A_FireCGun(player_t* player, pspdef_t* psp)
{
    Doom::fireCGun(player, psp);
}

void A_Light0(player_t* player, pspdef_t* psp)
{
    Doom::light0(player, psp);
}

void A_Light1(player_t* player, pspdef_t* psp)
{
    Doom::light1(player, psp);
}

void A_Light2(player_t* player, pspdef_t* psp)
{
    Doom::light2(player, psp);
}

void A_BFGSpray(mobj_t* mo)
{
    Doom::bfgSpray(mo);
}

void A_BFGsound(player_t* player, pspdef_t* psp)
{
    Doom::bfgSound(player, psp);
}

void P_SetupPsprites(player_t* player)
{
    Doom::setupPsprites(player);
}

void P_MovePsprites(player_t* player)
{
    Doom::movePsprites(player);
}

void P_DropWeapon(player_t* player)
{
    Doom::dropWeapon(player);
}
