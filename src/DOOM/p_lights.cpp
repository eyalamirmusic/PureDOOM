// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Sector lighting specials. Rewritten in Sim/Lights.{h,cpp}; this keeps the
//        vanilla names as shims. The T_ thinker functions stay global (p_saveg
//        identifies thinkers by comparing to their address).
//
//-----------------------------------------------------------------------------

#include "p_local.h"

#include "Sim/Lights.h"

void T_FireFlicker(fireflicker_t* flick)
{
    Doom::fireFlicker(flick);
}

void P_SpawnFireFlicker(sector_t* sector)
{
    Doom::spawnFireFlicker(sector);
}

void T_LightFlash(lightflash_t* flash)
{
    Doom::lightFlash(flash);
}

void P_SpawnLightFlash(sector_t* sector)
{
    Doom::spawnLightFlash(sector);
}

void T_StrobeFlash(strobe_t* flash)
{
    Doom::strobeFlash(flash);
}

void P_SpawnStrobeFlash(sector_t* sector, int fastOrSlow, int inSync)
{
    Doom::spawnStrobeFlash(sector, fastOrSlow, inSync);
}

void EV_StartLightStrobing(line_t* line)
{
    Doom::startLightStrobing(line);
}

void EV_TurnTagLightsOff(line_t* line)
{
    Doom::turnTagLightsOff(line);
}

void EV_LightTurnOn(line_t* line, int bright)
{
    Doom::lightTurnOn(line, bright);
}

void T_Glow(glow_t* g)
{
    Doom::glow(g);
}

void P_SpawnGlowingLight(sector_t* sector)
{
    Doom::spawnGlowingLight(sector);
}
