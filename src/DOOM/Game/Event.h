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

#include "../doomtype.h"

//
// Doom::Event handling.
//

// Input event types.
namespace Doom
{
enum class EventType
{
    KeyDown,
    KeyUp,
    Mouse,
    Joystick
};
} // namespace Doom

// Doom::Event structure.
namespace Doom
{
struct Event
{
    EventType type;
    int data1; // keys / mouse/joystick buttons
    int data2; // mouse/joystick x move
    int data3; // mouse/joystick y move
};
} // namespace Doom

namespace Doom
{
enum class GameAction
{
    Nothing,
    LoadLevel,
    NewGame,
    LoadGame,
    SaveGame,
    PlayDemo,
    Completed,
    Victory,
    WorldDone,
    Screenshot
};
} // namespace Doom

//
// Doom::Button/action code definitions.
//
namespace Doom
{
// Vanilla kept BT_* and BTS_* in one enum, which hid that they are two different
// vocabularies for the same byte: BTS_PAUSE and BT_ATTACK are both bit 0, and which
// meaning applies depends on whether BT_SPECIAL is set. Scoping them together would
// have made that collision look deliberate, so they are two enums - and the masks
// and shift counts, which are neither buttons nor commands, are constants.
enum class ButtonCode
{
    // Press "Fire".
    Attack = 1,
    // Use button, to open doors, activate switches.
    Use = 2,
    // Flag, weapon change pending.
    // If true, the next 3 bits hold weapon num.
    Change = 4,
    // Flag: game events, not really buttons.
    Special = 128
};

// What the low bits mean when ButtonCode::Special is set.
enum class SpecialCommand
{
    // Pause the game.
    Pause = 1,
    // Save the game at each console.
    SaveGame = 2
};

// The field masks and shifts packed into the same byte.
constexpr int buttonSpecialMask = 3;
// The 3bit weapon mask and shift, convenience.
constexpr int buttonWeaponMask = 8 + 16 + 32;
constexpr int buttonWeaponShift = 3;
// Savegame slot numbers occupy the second byte of buttons.
constexpr int buttonSaveMask = 4 + 8 + 16;
constexpr int buttonSaveShift = 2;
} // namespace Doom

//
// GLOBAL VARIABLES
//
namespace Doom
{
// [pd] Crank up the number because we pump them faster.
constexpr int MAXEVENTS = 64 * 64;
} // namespace Doom

// The input event ring buffer is a Doom::EventQueue owned by the Engine now; these are
// references onto its members (REFACTOR.md, Step 5).

// The pending game action is a member of the Doom::GameFlow owned by the Engine now; this is
// a reference onto it (REFACTOR.md, Step 5).

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
