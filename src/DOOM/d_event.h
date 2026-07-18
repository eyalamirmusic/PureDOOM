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
//
//    
//-----------------------------------------------------------------------------

#pragma once


#include "doomtype.h"


//
// Doom::Event handling.
//

// Input event types.
namespace Doom
{
enum EventType
{
    ev_keydown,
    ev_keyup,
    ev_mouse,
    ev_joystick
};
} // namespace Doom


// Doom::Event structure.
namespace Doom
{
struct Event
{
    EventType type;
    int data1;  // keys / mouse/joystick buttons
    int data2;  // mouse/joystick x move
    int data3;  // mouse/joystick y move
};
} // namespace Doom


namespace Doom
{
enum GameAction
{
    ga_nothing,
    ga_loadlevel,
    ga_newgame,
    ga_loadgame,
    ga_savegame,
    ga_playdemo,
    ga_completed,
    ga_victory,
    ga_worlddone,
    ga_screenshot
};
} // namespace Doom


//
// Doom::Button/action code definitions.
//
namespace Doom
{
enum ButtonCode
{
    // Press "Fire".
    BT_ATTACK = 1,
    // Use button, to open doors, activate switches.
    BT_USE = 2,

    // Flag: game events, not really buttons.
    BT_SPECIAL = 128,
    BT_SPECIALMASK = 3,

    // Flag, weapon change pending.
    // If true, the next 3 bits hold weapon num.
    BT_CHANGE = 4,
    // The 3bit weapon mask and shift, convenience.
    BT_WEAPONMASK = (8 + 16 + 32),
    BT_WEAPONSHIFT = 3,

    // Pause the game.
    BTS_PAUSE = 1,
    // Save the game at each console.
    BTS_SAVEGAME = 2,

    // Savegame slot numbers
    //  occupy the second byte of buttons.    
    BTS_SAVEMASK = (4 + 8 + 16),
    BTS_SAVESHIFT = 2,
};
} // namespace Doom


//
// GLOBAL VARIABLES
//
#define MAXEVENTS (64 * 64) // [pd] Crank up the number because we pump them faster

// The input event ring buffer is a Doom::EventQueue owned by the Engine now; these are
// references onto its members (REFACTOR.md, Step 5).

// The pending game action is a member of the Doom::GameFlow owned by the Engine now; this is
// a reference onto it (REFACTOR.md, Step 5).



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
