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
//  Internally used data structures for virtually everything,
//   key definitions, lots of other stuff.
//
//-----------------------------------------------------------------------------

#pragma once

#include "../doomtype.h" // toIndex, for the enum-derived counts below

//
// Global parameters/defines.
//
// DOOM version
namespace Doom
{
constexpr int VERSION = 110;
} // namespace Doom

// Game mode handling - identify IWAD version
//  to handle IWAD dependend animations etc.
namespace Doom
{
enum class GameMode
{
    Shareware, // DOOM 1 shareware, E1, M9
    Registered, // DOOM 1 registered, E3, M27
    Commercial, // DOOM 2 retail, E1 M34
    // DOOM 2 german edition not handled
    Retail, // DOOM 1 retail, E4, M36
    Indetermined // Well, no IWAD found.
};
} // namespace Doom

// Mission packs - might be useful for TC stuff?
namespace Doom
{
enum class GameMission
{
    Doom, // DOOM 1
    Doom2, // DOOM 2
    PackTnt, // TNT mission pack
    PackPlut, // Plutonia pack
    None
};
} // namespace Doom

// Identify language to use, software localization.
namespace Doom
{
enum class Language
{
    English,
    French,
    German,
    Unknown
};
} // namespace Doom

// If rangecheck is undefined,
// most parameter validation debugging code will not be compiled.
// This one stays a macro: it is a feature toggle read by #ifdef, not a value.
#define RANGECHECK

namespace Doom
{
//
// For resize of screen, at start of game.
// It will not work dynamically, see visplanes.
//
constexpr int BASE_WIDTH = 320;

// It is educational but futile to change this
//  scaling e.g. to 2. Drawing of status bar,
//  menues etc. is tied to the scale implied
//  by the graphics.
constexpr int SCREEN_MUL = 1;

constexpr int SCREENWIDTH = 320;
constexpr int SCREENHEIGHT = 200;

// The maximum number of players, multiplayer/networking.
constexpr int MAXPLAYERS = 4;

// Doom::State updates, number of tics / second.
#if defined(DOOM_FAST_TICK)
constexpr int TICKMUL = 2;
#else
constexpr int TICKMUL = 1;
#endif
constexpr int TICRATE = 35 * TICKMUL;
} // namespace Doom

// The current state of the game: whether we are
// playing, gazing at the intermission screen,
// the game final animation, or a demo.
namespace Doom
{
// The underlying type is stated because the enum also carries GS_FORCE_WIPE
// below, which is outside the enumerators' range - and without a fixed type,
// holding a value outside that range is UB (UBSan flagged every load of it).
enum class GameState : int
{
    Level,
    Intermission,
    Finale,
    DemoScreen
};

// The force-wipe/redraw sentinel: no real screen, written to wipegamestate (and
// to DisplayState::oldgamestate) so the next displayFrame sees a state change
// whatever the real state is. Vanilla spelled it as a bare -1 at every site.
// Deliberately not an enumerator: the switches over gamestate handle exactly
// the four real screens, and a fifth case would put an unreachable arm in each.
constexpr auto GS_FORCE_WIPE = static_cast<GameState>(-1);
} // namespace Doom

//
// Difficulty/skill settings/filters.
//

namespace Doom
{
// Doom::Skill flags.
constexpr int MTF_EASY = 1;
constexpr int MTF_NORMAL = 2;
constexpr int MTF_HARD = 4;

// Deaf monsters/do not react to sound.
constexpr int MTF_AMBUSH = 8;
} // namespace Doom

namespace Doom
{
enum class Skill
{
    Baby,
    Easy,
    Medium,
    Hard,
    Nightmare
};
} // namespace Doom

//
// Key cards.
//
namespace Doom
{
enum class Card
{
    BlueCard,
    YellowCard,
    RedCard,
    BlueSkull,
    YellowSkull,
    RedSkull,
    NumCards
};

// The count, for array sizes and loop bounds. Derived from the enum's own sentinel so
// the two cannot drift - the rule the fixed-size-array bounds follow elsewhere.
constexpr int numCards = toIndex(Card::NumCards);
} // namespace Doom

// The defined weapons,
// including a marker indicating
// user has not changed weapon.
namespace Doom
{
enum class WeaponType
{
    Fist,
    Pistol,
    Shotgun,
    Chaingun,
    Missile,
    Plasma,
    Bfg,
    Chainsaw,
    SuperShotgun,
    NumWeapons,
    // No pending weapon change.
    NoChange
};

constexpr int numWeapons = toIndex(WeaponType::NumWeapons);
} // namespace Doom

// Ammunition types defined.
namespace Doom
{
enum class AmmoType
{
    Clip, // Pistol / chaingun ammo.
    Shell, // Shotgun / double barreled shotgun.
    Cell, // Plasma rifle, BFG.
    Misl, // Missile launcher.
    NumAmmo,
    NoAmmo // Unlimited for chainsaw / fist.
};

constexpr int numAmmo = toIndex(AmmoType::NumAmmo);
} // namespace Doom

// Power up artifacts.
namespace Doom
{
enum class PowerType
{
    Invulnerability,
    Strength,
    Invisibility,
    IronFeet,
    AllMap,
    Infrared,
    NumPowers
};

constexpr int numPowers = toIndex(PowerType::NumPowers);
} // namespace Doom

//
// Power up durations,
//  how many seconds till expiration,
//  assuming TICRATE is 35 ticks/second.
//
namespace Doom
{
// Not an `enum class`: these are tic counts, assigned straight into Player::powers[]
// and counted down. They are a group of integer constants rather than an enumeration
// of alternatives, and typing them would force a toIndex() on every use - naming an
// index where a duration is meant. So they become constexpr ints, which is what the
// rest of this file's constants already are, and no raw enum is left behind.
constexpr int invulnTics = 30 * TICRATE;
constexpr int invisTics = 60 * TICRATE;
constexpr int infraTics = 120 * TICRATE;
constexpr int ironTics = 60 * TICRATE;
} // namespace Doom

//
// DOOM keyboard definition.
// This is the stuff configured by Setup.Exe.
// Most key data are simple ascii (uppercased).
//
namespace Doom
{
constexpr int KEY_RIGHTARROW = 0xae;
constexpr int KEY_LEFTARROW = 0xac;
constexpr int KEY_UPARROW = 0xad;
constexpr int KEY_DOWNARROW = 0xaf;
constexpr int KEY_ESCAPE = 27;
constexpr int KEY_ENTER = 13;
constexpr int KEY_TAB = 9;
constexpr int KEY_F1 = 0x80 + 0x3b;
constexpr int KEY_F2 = 0x80 + 0x3c;
constexpr int KEY_F3 = 0x80 + 0x3d;
constexpr int KEY_F4 = 0x80 + 0x3e;
constexpr int KEY_F5 = 0x80 + 0x3f;
constexpr int KEY_F6 = 0x80 + 0x40;
constexpr int KEY_F7 = 0x80 + 0x41;
constexpr int KEY_F8 = 0x80 + 0x42;
constexpr int KEY_F9 = 0x80 + 0x43;
constexpr int KEY_F10 = 0x80 + 0x44;
constexpr int KEY_F11 = 0x80 + 0x57;
constexpr int KEY_F12 = 0x80 + 0x58;

constexpr int KEY_BACKSPACE = 127;
constexpr int KEY_PAUSE = 0xff;

constexpr int KEY_EQUALS = 0x3d;
constexpr int KEY_MINUS = 0x2d;

constexpr int KEY_RSHIFT = 0x80 + 0x36;
constexpr int KEY_RCTRL = 0x80 + 0x1d;
constexpr int KEY_RALT = 0x80 + 0x38;

constexpr int KEY_LALT = KEY_RALT;
} // namespace Doom

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
