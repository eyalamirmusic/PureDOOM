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

#include <string_view>

// The player data structure depends on a number
// of other structs: items (internal inventory),
// animation states (closely tied to the sprites
// used to represent them, unfortunately).
#include "../Sim/ItemTypes.h"
#include "../Sim/WeaponTypes.h"

// In addition, the player is just a special
// case of the generic moving object/actor.
#include "../Sim/MobjTypes.h"

// Finally, for odd reasons, the player input
// is buffered within the player data struct,
// as commands per game tick.
#include "Ticcmd.h"

//
// Doom::Player states.
//
namespace Doom
{
enum class PlayerLifeState
{
    // Playing or camping.
    Live,
    // Dead on the ground, view follows killer.
    Dead,
    // Ready to restart/respawn???
    Reborn
};
} // namespace Doom

//
// Doom::Player internal flags, for cheats and debug.
//
namespace Doom
{
enum CheatFlag
{
    // No clipping, walk through barriers.
    CF_NOCLIP = 1,
    // No damage, no health loss.
    CF_GODMODE = 2,
    // Not really a cheat, just a debug aid.
    CF_NOMOMENTUM = 4
};
} // namespace Doom

//
// Extended player object info: Doom::Player
//
namespace Doom
{
struct Player
{
    Mobj* mo;
    PlayerLifeState playerstate;
    Ticcmd cmd;

    // Determine POV,
    //  including viewpoint bobbing during movement.
    // Focal origin above r.z
    fixed_t viewz;
    // Base height above floor for viewz.
    fixed_t viewheight;
    // Bob/squat speed.
    fixed_t deltaviewheight;
    // bounded/scaled total momentum.
    fixed_t bob;

    // This is only used between levels,
    // mo->health is used during levels.
    int health;
    int armorpoints;
    // Armor type is 0-2.
    int armortype;

    // Power ups. invinc and invis are tic counters.
    int powers[numPowers];
    bool cards[numCards];
    bool backpack;

    // Frags, kills of other players.
    int frags[MAXPLAYERS];
    WeaponType readyweapon;

    // Is WeaponType::NoChange if not changing.
    WeaponType pendingweapon;

    bool weaponowned[numWeapons];
    int ammo[numAmmo];
    int maxammo[numAmmo];

    // True if button down last tic.
    int attackdown;
    int usedown;

    // Bit flags, for cheats and debug.
    // See CheatFlag, above.
    int cheats;

    // Refired shots are less accurate.
    int refire;

    // For intermission stats.
    int killcount;
    int itemcount;
    int secretcount;

    // The hint message shown on the HUD this tic, and cleared by the HUD once it
    // has been drawn. A non-owning view, so whatever is assigned to it must outlive
    // the frame: every writer passes a string constant or an Engine-owned
    // std::string. unArchivePlayers clears it after the memcpy, alongside mo and
    // attacker, so a loaded game never inherits a stale one.
    std::string_view message;

    // For screen flashing (red or bright).
    int damagecount;
    int bonuscount;

    // Who did damage (0 for floors/ceilings).
    Mobj* attacker;

    // So gun flashes light up areas.
    int extralight;

    // Current PLAYPAL, ???
    //  can be set to REDCOLORMAP for pain, etc.
    int fixedcolormap;

    // Player skin colorshift,
    //  0-3 for which color to draw player.
    int colormap;

    // Overlay view sprites (gun, etc).
    PspDef psprites[numPSprites];

    // True if secret level has been done.
    bool didsecret;
};
} // namespace Doom

//
// INTERMISSION
// Structure passed e.g. to Doom::startIntermission(wb)
//
namespace Doom
{
struct IntermissionPlayer
{
    bool in; // whether the player is in game

    // Player stats, kills, collected items etc.
    int skills;
    int sitems;
    int ssecret;
    int stime;
    int frags[4];
    int score; // current score on entry, modified on return
};
} // namespace Doom

namespace Doom
{
struct IntermissionStart
{
    int epsd; // episode # (0-2)

    // if true, splash the secret level
    bool didsecret;

    // previous and next levels, origin 0
    int last;
    int next;

    int maxkills;
    int maxitems;
    int maxsecret;
    int maxfrags;

    // the par time
    int partime;

    // index of this player in game
    int pnum;

    IntermissionPlayer plyr[MAXPLAYERS];
};
} // namespace Doom

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
