// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// DESCRIPTION:
//        Enemy thinking, AI. Rewritten in Sim/Enemy.{h,cpp}; this keeps the vanilla
//        A_*/P_NoiseAlert names as shims (info.cpp references the A_* by address)
//        and the soundtarget global that p_saveg archives.
//
//-----------------------------------------------------------------------------

#include "d_player.h"
#include "p_local.h"
#include "p_mobj.h"

#include "Sim/Enemy.h"
#include "Sim/SoundTarget.h"

// The last thing that made noise, propagated to nearby monsters. A Doom::SoundTarget owned by the
// Engine now (Sim/SoundTarget.h); this is a reference onto it. (The old "p_saveg archives it" note
// was wrong - p_saveg only touches sector_t::soundtarget, never this global.) Written by
// Doom::noiseAlert / recursiveSound.
mobj_t*& soundtarget = Doom::soundTarget().soundtarget;

void A_KeenDie(mobj_t* mo)
{
    Doom::keenDie(*mo);
}

void A_Look(mobj_t* actor)
{
    Doom::look(*actor);
}

void A_Chase(mobj_t* actor)
{
    Doom::chase(*actor);
}

void A_FaceTarget(mobj_t* actor)
{
    Doom::faceTarget(*actor);
}

void A_PosAttack(mobj_t* actor)
{
    Doom::posAttack(*actor);
}

void A_SPosAttack(mobj_t* actor)
{
    Doom::sPosAttack(*actor);
}

void A_CPosAttack(mobj_t* actor)
{
    Doom::cPosAttack(*actor);
}

void A_CPosRefire(mobj_t* actor)
{
    Doom::cPosRefire(*actor);
}

void A_SpidRefire(mobj_t* actor)
{
    Doom::spidRefire(*actor);
}

void A_BspiAttack(mobj_t* actor)
{
    Doom::bspiAttack(*actor);
}

void A_TroopAttack(mobj_t* actor)
{
    Doom::troopAttack(*actor);
}

void A_SargAttack(mobj_t* actor)
{
    Doom::sargAttack(*actor);
}

void A_HeadAttack(mobj_t* actor)
{
    Doom::headAttack(*actor);
}

void A_CyberAttack(mobj_t* actor)
{
    Doom::cyberAttack(*actor);
}

void A_BruisAttack(mobj_t* actor)
{
    Doom::bruisAttack(*actor);
}

void A_SkelMissile(mobj_t* actor)
{
    Doom::skelMissile(*actor);
}

void A_Tracer(mobj_t* actor)
{
    Doom::tracer(*actor);
}

void A_SkelWhoosh(mobj_t* actor)
{
    Doom::skelWhoosh(*actor);
}

void A_SkelFist(mobj_t* actor)
{
    Doom::skelFist(*actor);
}

void A_VileChase(mobj_t* actor)
{
    Doom::vileChase(*actor);
}

void A_VileStart(mobj_t* actor)
{
    Doom::vileStart(*actor);
}

void A_StartFire(mobj_t* actor)
{
    Doom::startFire(*actor);
}

void A_FireCrackle(mobj_t* actor)
{
    Doom::fireCrackle(*actor);
}

void A_Fire(mobj_t* actor)
{
    Doom::fire(*actor);
}

void A_VileTarget(mobj_t* actor)
{
    Doom::vileTarget(*actor);
}

void A_VileAttack(mobj_t* actor)
{
    Doom::vileAttack(*actor);
}

void A_FatRaise(mobj_t* actor)
{
    Doom::fatRaise(*actor);
}

void A_FatAttack1(mobj_t* actor)
{
    Doom::fatAttack1(*actor);
}

void A_FatAttack2(mobj_t* actor)
{
    Doom::fatAttack2(*actor);
}

void A_FatAttack3(mobj_t* actor)
{
    Doom::fatAttack3(*actor);
}

void A_SkullAttack(mobj_t* actor)
{
    Doom::skullAttack(*actor);
}

void A_PainShootSkull(mobj_t* actor, angle_t angle)
{
    Doom::painShootSkull(*actor, angle);
}

void A_PainAttack(mobj_t* actor)
{
    Doom::painAttack(*actor);
}

void A_PainDie(mobj_t* actor)
{
    Doom::painDie(*actor);
}

void A_Scream(mobj_t* actor)
{
    Doom::scream(*actor);
}

void A_XScream(mobj_t* actor)
{
    Doom::xScream(*actor);
}

void A_Pain(mobj_t* actor)
{
    Doom::pain(*actor);
}

void A_Fall(mobj_t* actor)
{
    Doom::fall(*actor);
}

void A_Explode(mobj_t* thingy)
{
    Doom::explode(*thingy);
}

void A_BossDeath(mobj_t* mo)
{
    Doom::bossDeath(*mo);
}

void A_Hoof(mobj_t* mo)
{
    Doom::hoof(*mo);
}

void A_Metal(mobj_t* mo)
{
    Doom::metal(*mo);
}

void A_BabyMetal(mobj_t* mo)
{
    Doom::babyMetal(*mo);
}

void A_OpenShotgun2(player_t* player, pspdef_t* psp)
{
    Doom::openShotgun2(player, psp);
}

void A_LoadShotgun2(player_t* player, pspdef_t* psp)
{
    Doom::loadShotgun2(player, psp);
}

void A_CloseShotgun2(player_t* player, pspdef_t* psp)
{
    Doom::closeShotgun2(player, psp);
}

void A_BrainAwake(mobj_t* mo)
{
    Doom::brainAwake(*mo);
}

void A_BrainPain(mobj_t* mo)
{
    Doom::brainPain(*mo);
}

void A_BrainScream(mobj_t* mo)
{
    Doom::brainScream(*mo);
}

void A_BrainExplode(mobj_t* mo)
{
    Doom::brainExplode(*mo);
}

void A_BrainDie(mobj_t* mo)
{
    Doom::brainDie(*mo);
}

void A_BrainSpit(mobj_t* mo)
{
    Doom::brainSpit(*mo);
}

void A_SpawnSound(mobj_t* mo)
{
    Doom::spawnSound(*mo);
}

void A_SpawnFly(mobj_t* mo)
{
    Doom::spawnFly(*mo);
}

void A_PlayerScream(mobj_t* mo)
{
    Doom::playerScream(*mo);
}

void P_NoiseAlert(mobj_t* target, mobj_t* emmiter)
{
    Doom::noiseAlert(target, *emmiter);
}
