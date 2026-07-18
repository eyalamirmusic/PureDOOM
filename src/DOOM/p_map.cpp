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
//        Movement, collision handling.
//        Shooting and aiming.
//
//-----------------------------------------------------------------------------


#include "doom_config.h"



#include "m_bbox.h"
#include "m_random.h"
#include "i_system.h"
#include "doomdef.h"
#include "p_local.h"
#include "s_sound.h"
#include "doomstat.h" // State.
#include "r_state.h" // State.
#include "sounds.h" // Data.

#include "Sim/Clip.h"
#include "Sim/Level.h"
#include "Sim/MapAction.h"
#include "Sim/Movement.h"


// The movement-clipping state (vanilla's tm*) lives in Doom::Clip now, and the core
// clipping functions moved to Sim/Movement.{h,cpp}, which read it directly. These
// vanilla names are references onto Clip for the code not yet rewritten: p_enemy
// (spechit/numspechit/tmfloorz/floatok) and p_mobj (ceilingline).
doom_boolean& floatok = Doom::clip().floatok;
fixed_t& tmfloorz = Doom::clip().tmfloorz;
fixed_t& tmceilingz = Doom::clip().tmceilingz;
Doom::Line*& ceilingline = Doom::clip().ceilingline;
Doom::Line** spechit = Doom::clip().spechit;
int& numspechit = Doom::clip().numspechit;

// linetarget (the aim's hit) and attackrange (the shot's range) are read by p_mobj
// and p_pspr, so they live in Doom::Clip with vanilla-name references here. The rest
// of the slide/aim/shoot/radius scratch is file-local to Sim/MapAction.cpp.
Doom::Mobj*& linetarget = Doom::clip().linetarget;
fixed_t& attackrange = Doom::clip().attackrange;


//
// TELEPORT MOVE
// 

//
// P_TeleportMove
//


//
// MOVEMENT ITERATOR FUNCTIONS
//
// PIT_CheckLine, PIT_CheckThing and PIT_StompThing moved to Sim/Movement.cpp as
// file-local helpers of checkPosition/teleportMove.
//
//
// MOVEMENT CLIPPING
//

//
// P_CheckPosition
// This is purely informative, nothing is modified
// (except things picked up).
// 
// in:
//  a Doom::Mobj (can be valid or invalid)
//  a position to be checked
//   (doesn't need to be related to the Doom::Mobj->x,y)
//
// during:
//  special things are touched if MF_PICKUP
//  early out on solid lines?
//
// out:
//  newsubsec
//  floorz
//  ceilingz
//  tmdropoffz
//   the lowest point contacted
//   (monsters won't move to a dropoff)
//  speciallines[]
//  numspeciallines
//


//
// P_TryMove
// Attempt to move to a new position,
// crossing special lines unless MF_TELEPORT is set.
//


//
// P_ThingHeightClip
// Takes a valid thing and adjusts the thing->floorz,
// thing->ceilingz, and possibly thing->z.
// This is called for all nearby monsters
// whenever a sector changes height.
// If the thing doesn't fit,
// the z will be set to the lowest value
// and false will be returned.
//


//
// SLIDE MOVE, LINE ATTACK, USE LINES, RADIUS ATTACK, SECTOR HEIGHT CHANGING
// All rewritten in Sim/MapAction.{h,cpp}; these keep the vanilla names. The scratch
// they share (bestslidefrac, shootz, bombspot, ...) is file-local there; only the
// aim's linetarget and the shot's attackrange (above) are read from other files.
//











