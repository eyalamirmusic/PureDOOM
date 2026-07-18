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
//   All the global variables that store the internal state.
//   Theoretically speaking, the internal state of the engine
//    should be found by looking at the variables collected
//    here, and every relevant module will have to include
//    this header file.
//   In practice, things are a bit messy.
//
//-----------------------------------------------------------------------------

#pragma once


// We need globally shared data structures,
//  for defining the global state variables.
#include "doomdata.h"
#include "d_net.h"

// We need the playr data structure as well.
#include "d_player.h"


// ------------------------
// Command line parameters.
//
// The command-line launch flags live in Doom::LaunchOptions (an Engine member) now; these
// are references onto it (REFACTOR.md, Step 5).


// -----------------------------------------------------
// Game Mode - identify IWAD as shareware, retail etc.
//
// The loaded game's identity lives in Doom::GameVersion (an Engine member) now; these
// are references onto it (REFACTOR.md, Step 5).

// Set if homebrew PWAD stuff has been added.


// -------------------------------------------
// Doom::Language.


// -------------------------------------------
// Selected skill type, map etc.
//

// Defaults for menu, methinks. They live in Doom::StartupDefaults (an Engine member) now;
// these are references onto it (REFACTOR.md, Step 5).


// The current game's rules live in Doom::GameSession (an Engine member) now; these are
// references onto it (REFACTOR.md, Step 5).

// Selected by user.

// Nightmare mode flag, single player.

// Netgame? Only true if >1 player.

// Flag: true only if started as net deathmatch.
// An enum might handle altdeath/cooperative better.

// -------------------------
// Internal parameters for sound rendering.
// These have been taken from the DOS version,
// but are not (yet) supported with Linux
// (e.g. no sound volume adjustment with menu.

// The sfx/music volumes are config-backed and now owned by the Engine's SoundSettings
// cluster (Game/SoundSettings.h); these are references onto those members. Config.cpp
// binds its defaults[] entries to them at runtime rather than capturing their addresses
// at static-init, which is what unblocked the migration. The snd_*Device selectors that
// were declared here were always dead (no definition, no reader) and are dropped.


// -------------------------
// Status flags for refresh.
//

// statusbaractive was declared here but never defined or read - dropped (REFACTOR.md,
// Step 5).

// automapactive/menuactive live in Doom::OverlayState (an Engine member) now; these are
// references onto it (REFACTOR.md, Step 5).

// paused/viewactive/nodrawers/noblit live in Doom::RefreshFlags (an Engine member) now;
// these are references onto it (REFACTOR.md, Step 5).



// The view window geometry lives in Doom::ViewWindow (an Engine member) now; these
// are references onto it (REFACTOR.md, Step 5).


// This one is related to the 3-screen display mode.
// ANG90 = left side, ANG270 = right

// Doom::Player taking events, and displaying. These live in Doom::PlayerState (an Engine member)
// now, with the player arrays below; all four are references onto it (REFACTOR.md, Step 5).


// -------------------------------------
// Scores, rating.
// Statistics on a given map, for intermission.
//
// The level's progress lives in Doom::LevelStats (an Engine member) now; these are
// references onto it (REFACTOR.md, Step 5).

// Timer, for scores.


// --------------------------------------
// DEMO playback/recording related stuff.
// No demo, there is a human player in charge?
// Disable save/end game?
// The demo-playback state lives in Doom::DemoState (an Engine member) now; these are
// references onto it (REFACTOR.md, Step 5).

//?

// Quit after playing a demo from cmdline.

// gamestate and wipegamestate live in Doom::GameFlow (an Engine member) now; these are
// references onto it (REFACTOR.md, Step 5).


//-----------------------------
// Internal parameters, fixed.
// These are set by the engine, and not changed
// according to user inputs. Partly load from
// WAD, partly set at startup time.

// gametic lives in Doom::GameClock (an Engine member) now; this is a reference onto it
// (REFACTOR.md, Step 5).


// Bookkeeping on players - state. In Doom::PlayerState (an Engine member) now, with
// consoleplayer/displayplayer above; references-to-array onto it (REFACTOR.md, Step 5).

// Alive? Disconnected?


// Doom::Player spawn spots. These live in Doom::MapSpawns (an Engine member) now; the references
// onto it are references-to-array for the two arrays (REFACTOR.md, Step 5).
#define MAX_DM_STARTS 10


// Intermission stats. Parameters for world map / intermission. In Doom::IntermissionInfo
// (an Engine member) now; a reference onto it (REFACTOR.md, Step 5).


// LUT of ammunition limits for each kind. This doubles with BackPack powerup item.
// In Doom::AmmoLimits (an Engine member) now; a reference-to-array onto it, also declared in
// p_local.h (REFACTOR.md, Step 5).


//-----------------------------------------
// Internal parameters, used for engine.
//

// File handling stuff. basedefault is an Engine member (Game/ConfigPaths.h); reference.
// debugfile / precache / singletics are Doom::EngineParams (an Engine member) now; these are
// references onto it (REFACTOR.md, Step 5).

// if true, load all graphics at level load

// wipegamestate can be set to -1
// to force a wipe on the next draw (in Doom::GameFlow now; see gamestate above)

// mouseSensitivity is config-backed and owned by the Engine's MenuSettings cluster
// (UI/MenuSettings.h) now; this is a reference onto that member.
//?
// debug flag to cancel adaptiveness (in Doom::EngineParams now; see debugfile above)

// bodyqueslot lives in Doom::CorpseQueue (an Engine member) now, with the bodyque[] array;
// this is a reference onto it (REFACTOR.md, Step 5).


// Needed to store the number of the dummy sky flat. Used for rendering, as well as tracking
// projectiles etc. In Doom::SkyState (an Engine member) now; a reference onto it
// (REFACTOR.md, Step 5).


// Netgame stuff (buffers and pointers, i.e. indices). Lives in Doom::NetState (an Engine
// member) now; these are references onto it, the arrays as references-to-array
// (REFACTOR.md, Step 5).

// This is ???

// This points inside doomcom.



// rndindex was declared here too, a second name for m_random.h's. Include that
// instead - Doom::Random owns the state now and there is one declaration of it.





//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
