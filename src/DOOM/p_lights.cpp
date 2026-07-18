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
    Doom::fireFlicker(*flick);
}


void T_LightFlash(lightflash_t* flash)
{
    Doom::lightFlash(*flash);
}


void T_StrobeFlash(strobe_t* flash)
{
    Doom::strobeFlash(*flash);
}





void T_Glow(glow_t* g)
{
    Doom::glow(*g);
}

