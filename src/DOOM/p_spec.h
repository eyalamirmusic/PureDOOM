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
// DESCRIPTION:  none
//        Implements special effects:
//        Texture animation, height or lighting changes
//         according to adjacent sectors, respective
//         utility functions, etc.
//
//-----------------------------------------------------------------------------

#pragma once


#include "p_mobj.h"
#include "r_defs.h"


//
// End-level timer (-TIMER option)
//
// The end-level timer is a Doom::EndLevelTimer owned by the Engine now; these are references
// onto its members (REFACTOR.md, Step 5).
extern doom_boolean& levelTimer;
extern int& levelTimeCount;


// Define values for map objects
#define MO_TELEPORTMAN          14


// at game start

// at map load

// every tic

// when needed

int twoSided(int sector, int line);
Doom::Sector* getSector(int currentSector, int line, int side);
Doom::Side* getSide(int currentSector, int line, int side);
Doom::Sector* getNextSector(Doom::Line* line, Doom::Sector* sec);

//
// SPECIAL
//


//
// P_LIGHTS
//
struct fireflicker_t : Doom::Thinker
{
    void tick() override;
    Doom::ThinkerKind kind() const override { return Doom::ThinkerKind::FireFlicker; }
    Doom::Sector* sector;
    int count;
    int maxlight;
    int minlight;
};


struct lightflash_t : Doom::Thinker
{
    void tick() override;
    Doom::ThinkerKind kind() const override { return Doom::ThinkerKind::LightFlash; }
    Doom::Sector* sector;
    int count;
    int maxlight;
    int minlight;
    int maxtime;
    int mintime;
};


struct strobe_t : Doom::Thinker
{
    void tick() override;
    Doom::ThinkerKind kind() const override { return Doom::ThinkerKind::StrobeFlash; }
    Doom::Sector* sector;
    int count;
    int minlight;
    int maxlight;
    int darktime;
    int brighttime;
};


struct glow_t : Doom::Thinker
{
    void tick() override;
    Doom::ThinkerKind kind() const override { return Doom::ThinkerKind::Glow; }
    Doom::Sector* sector;
    int minlight;
    int maxlight;
    int direction;
};


#define GLOWSPEED 8
#define STROBEBRIGHT 5
#define FASTDARK 15
#define SLOWDARK 35


//
// P_SWITCH
//
struct switchlist_t
{
    char name1[9];
    char name2[9];
    short episode;
};


enum bwhere_e
{
    top,
    middle,
    bottom
};


struct button_t
{
    Doom::Line* line;
    bwhere_e where;
    int btexture;
    int btimer;
    mobj_t* soundorg;
};


// max # of wall switches in a level
#define MAXSWITCHES 50

// 4 players, 4 buttons each at once, max.
#define MAXBUTTONS 16

// 1 second, in ticks. 
#define BUTTONTIME 35             


// The active-special registries are a Doom::ActiveSpecials owned by the Engine now; these
// (and activeplats/activeceilings below) are references onto its members (REFACTOR.md, Step 5).
extern button_t (&buttonlist)[MAXBUTTONS];




//
// P_PLATS
//
enum plat_e
{
    up,
    down,
    waiting,
    in_stasis
};


enum plattype_e
{
    perpetualRaise,
    downWaitUpStay,
    raiseAndChange,
    raiseToNearestAndChange,
    blazeDWUS
};


struct plat_t : Doom::Thinker
{
    void tick() override;
    Doom::ThinkerKind kind() const override { return Doom::ThinkerKind::Plat; }
    Doom::Sector* sector;
    fixed_t speed;
    fixed_t low;
    fixed_t high;
    int wait;
    int count;
    plat_e status;
    plat_e oldstatus;
    doom_boolean crush;
    int tag;
    plattype_e type;
};


#define PLATWAIT 3
#define PLATSPEED FRACUNIT
#define MAXPLATS 30


extern plat_t* (&activeplats)[MAXPLATS];


//
// P_DOORS
//
enum vldoor_e
{
    door_normal,
    close30ThenOpen,
    door_close,
    door_open,
    raiseIn5Mins,
    blazeRaise,
    blazeOpen,
    blazeClose
};


struct vldoor_t : Doom::Thinker
{
    void tick() override;
    Doom::ThinkerKind kind() const override { return Doom::ThinkerKind::Door; }
    vldoor_e type;
    Doom::Sector* sector;
    fixed_t topheight;
    fixed_t speed;

    // 1 = up, 0 = waiting at top, -1 = down
    int direction;

    // tics to wait at the top
    int topwait;

    // (keep in case a door going down is reset)
    // when it reaches 0, start going down
    int topcountdown;
};


#define VDOORSPEED                FRACUNIT*2
#define VDOORWAIT                150


//
// P_CEILNG
//
enum ceiling_e
{
    lowerToFloor,
    raiseToHighest,
    lowerAndCrush,
    crushAndRaise,
    fastCrushAndRaise,
    silentCrushAndRaise
};


struct ceiling_t : Doom::Thinker
{
    void tick() override;
    Doom::ThinkerKind kind() const override { return Doom::ThinkerKind::Ceiling; }
    ceiling_e type;
    Doom::Sector* sector;
    fixed_t bottomheight;
    fixed_t topheight;
    fixed_t speed;
    doom_boolean crush;

    // 1 = up, 0 = waiting, -1 = down
    int direction;

    // ID
    int tag;
    int olddirection;
};


#define CEILSPEED FRACUNIT
#define CEILWAIT 150
#define MAXCEILINGS 30


extern ceiling_t* (&activeceilings)[MAXCEILINGS];


//
// P_FLOOR
//
enum floor_e
{
    // lower floor to highest surrounding floor
    lowerFloor,

    // lower floor to lowest surrounding floor
    lowerFloorToLowest,

    // lower floor to highest surrounding floor VERY FAST
    turboLower,

    // raise floor to lowest surrounding CEILING
    raiseFloor,

    // raise floor to next highest surrounding floor
    raiseFloorToNearest,

    // raise floor to shortest height texture around it
    raiseToTexture,

    // lower floor to lowest surrounding floor
    //  and change floorpic
    lowerAndChange,

    raiseFloor24,
    raiseFloor24AndChange,
    raiseFloorCrush,

    // raise to next highest floor, turbo-speed
    raiseFloorTurbo,
    donutRaise,
    raiseFloor512
};


enum stair_e
{
    build8,        // slowly build by 8
    turbo16        // quickly build by 16
};


struct floormove_t : Doom::Thinker
{
    void tick() override;
    Doom::ThinkerKind kind() const override { return Doom::ThinkerKind::Floor; }
    floor_e type;
    doom_boolean crush;
    Doom::Sector* sector;
    int direction;
    int newspecial;
    short texture;
    fixed_t floordestheight;
    fixed_t speed;
};


#define FLOORSPEED FRACUNIT


enum result_e
{
    ok,
    crushed,
    pastdest
};


result_e T_MovePlane(Doom::Sector* sector, fixed_t speed, fixed_t dest, doom_boolean crush, int floorOrCeiling, int direction);

//
// P_TELEPT
//



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
