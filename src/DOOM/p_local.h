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
//        Play functions, animation, global header.
//
//-----------------------------------------------------------------------------

#pragma once


#ifndef __R_LOCAL__
#include "r_local.h"
#endif


#define FLOATSPEED (FRACUNIT*4)

#define MAXHEALTH 100
#define VIEWHEIGHT (41*FRACUNIT)

// mapblocks are used to check movement
// against lines and things
#define MAPBLOCKUNITS 128
#define MAPBLOCKSIZE (MAPBLOCKUNITS*FRACUNIT)
#define MAPBLOCKSHIFT (FRACBITS+7)
#define MAPBMASK (MAPBLOCKSIZE-1)
#define MAPBTOFRAC (MAPBLOCKSHIFT-FRACBITS)


// player radius for movement checking
#define PLAYERRADIUS 16*FRACUNIT

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS 32*FRACUNIT

#define GRAVITY FRACUNIT
#define MAXMOVE (30*FRACUNIT)

#define USERANGE (64*FRACUNIT)
#define MELEERANGE (64*FRACUNIT)
#define MISSILERANGE (32*64*FRACUNIT)

// follow a player exlusively for 3 seconds
#define BASETHRESHOLD 100


//
// P_TICK
//

// both the head and tail of the thinker list; a reference onto Doom::ThinkerList's cap
// (an Engine member) - the storage moved off this loose global in Step 5
extern Doom::Thinker& thinkercap;




//
// P_PSPR
//
void P_SetupPsprites(Doom::Player* curplayer);
void P_MovePsprites(Doom::Player* curplayer);
void P_DropWeapon(Doom::Player* player);


//
// P_USER
//
void P_PlayerThink(Doom::Player* player);


//
// P_MOBJ
//
#define ONFLOORZ DOOM_MININT
#define ONCEILINGZ DOOM_MAXINT

// Time interval for item respawning.
#define ITEMQUESIZE                128


// The item respawn queue lives in Doom::ItemRespawnQueue (an Engine member) now; these are
// references onto it, the arrays as references-to-array (REFACTOR.md, Step 5).
extern Doom::MapThing (&itemrespawnque)[ITEMQUESIZE];
extern int (&itemrespawntime)[ITEMQUESIZE];
extern int& iquehead;
extern int& iquetail;




//
// P_ENEMY
//


//
// P_MAPUTL
//
struct divline_t
{
    fixed_t x;
    fixed_t y;
    fixed_t dx;
    fixed_t dy;
};


namespace Doom
{
struct Intercept
{
    fixed_t frac; // along trace line
    doom_boolean isaline;
    union
    {
        Doom::Mobj* thing;
        Doom::Line* line;
    } d;
};
} // namespace Doom


#define MAXINTERCEPTS        128
// intercepts[] and intercept_p moved into Doom::Clip (Sim/Clip.h), reached through
// Doom::clip(); they were p_maputl's own scratch, read by no other file.


typedef doom_boolean(*Traverser) (Doom::Intercept* in);


fixed_t P_AproxDistance(fixed_t dx, fixed_t dy);
int P_PointOnLineSide(fixed_t x, fixed_t y, Doom::Line* line);
int P_PointOnDivlineSide(fixed_t x, fixed_t y, divline_t* line);
void P_MakeDivline(Doom::Line* li, divline_t* dl);
fixed_t P_InterceptVector(divline_t* v2, divline_t* v1);
int P_BoxOnLineSide(fixed_t* tmbox, Doom::Line* ld);


// References into Doom::Clip (Sim/Clip.h), reached through Doom::clip() - the same
// vanilla-name-over-owning-object shim rndindex/prndindex use. The clipping code
// still reads these as globals; they become clip().opentop directly once p_map /
// p_sight / p_enemy are rewritten to take an Engine&.
extern fixed_t& opentop;
extern fixed_t& openbottom;
extern fixed_t& openrange;
extern fixed_t& lowfloor;


void P_LineOpening(Doom::Line* linedef);


doom_boolean P_BlockLinesIterator(int x, int y, doom_boolean(*func)(Doom::Line*));
doom_boolean P_BlockThingsIterator(int x, int y, doom_boolean(*func)(Doom::Mobj*));


#define PT_ADDLINES     1
#define PT_ADDTHINGS    2
#define PT_EARLYOUT     4


extern divline_t& trace;


doom_boolean P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, int flags, doom_boolean(*trav) (Doom::Intercept*));
void P_UnsetThingPosition(Doom::Mobj* thing);
void P_SetThingPosition(Doom::Mobj* thing);


//
// P_MAP
//

// If "floatok" true, move would be ok if within "tmfloorz - tmceilingz".
// These are references into Doom::Clip now (see Sim/Movement.h); p_enemy reads
// floatok/tmfloorz, p_mobj reads ceilingline.
extern doom_boolean& floatok;
extern fixed_t& tmfloorz;
extern fixed_t& tmceilingz;

extern Doom::Line*& ceilingline;




extern Doom::Mobj*& linetarget; // who got hit (or 0); a reference into Doom::Clip




//
// P_SETUP
//
extern byte* rejectmatrix; // for fast sight rejection
extern short* blockmaplump; // offsets in blockmap are from here
extern short* blockmap;
extern int bmapwidth;
extern int bmapheight; // in mapblocks
extern fixed_t bmaporgx;
extern fixed_t bmaporgy; // origin of block map
extern Doom::Mobj** blocklinks; // for thing chains


//
// P_INTER
//
extern int (&maxammo)[Doom::NUMAMMO]; // Doom::AmmoLimits (Engine member)
extern int (&clipammo)[Doom::NUMAMMO]; // Doom::AmmoLimits (Engine member)




//
// P_SPEC
//
#include "p_spec.h"



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
