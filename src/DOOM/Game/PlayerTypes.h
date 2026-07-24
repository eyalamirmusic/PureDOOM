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
enum class CheatFlag
{
    // No clipping, walk through barriers.
    NoClip = 1,
    // No damage, no health loss.
    GodMode = 2,
    // Not really a cheat, just a debug aid.
    NoMomentum = 4
};
} // namespace Doom

//
// Extended player object info: Doom::Player
//
namespace Doom
{
struct Player
{
    Mobj* mo = nullptr;
    PlayerLifeState playerstate = PlayerLifeState::Live;
    Ticcmd cmd;

    // Determine POV,
    //  including viewpoint bobbing during movement.
    // Focal origin above r.z
    Fixed viewz;
    // Base height above floor for viewz.
    Fixed viewheight;
    // Bob/squat speed.
    Fixed deltaviewheight;
    // bounded/scaled total momentum.
    Fixed bob;

    // This is only used between levels,
    // mo->health is used during levels.
    int health = 0;
    int armorpoints = 0;
    // Armor type is 0-2.
    int armortype = 0;

    // Power ups. invinc and invis are tic counters.
    int powers[numPowers] = {};
    bool cards[numCards] = {};
    bool backpack = false;

    // Frags, kills of other players.
    int frags[MAXPLAYERS] = {};
    WeaponType readyweapon = WeaponType::Fist;

    // Is WeaponType::NoChange if not changing.
    WeaponType pendingweapon = WeaponType::Fist;

    bool weaponowned[numWeapons] = {};
    int ammo[numAmmo] = {};
    int maxammo[numAmmo] = {};

    // True if button down last tic.
    int attackdown = 0;
    int usedown = 0;

    // Bit flags, for cheats and debug.
    // See CheatFlag, above.
    int cheats = 0;

    // Refired shots are less accurate.
    int refire = 0;

    // For intermission stats.
    int killcount = 0;
    int itemcount = 0;
    int secretcount = 0;

    // The hint message shown on the HUD this tic, and cleared by the HUD once it
    // has been drawn. A non-owning view, so whatever is assigned to it must outlive
    // the frame: every writer passes a string constant or an Engine-owned
    // std::string. unArchivePlayers clears it after the memcpy, alongside mo and
    // attacker, so a loaded game never inherits a stale one.
    std::string_view message;

    // For screen flashing (red or bright).
    int damagecount = 0;
    int bonuscount = 0;

    // Who did damage (0 for floors/ceilings).
    Mobj* attacker = nullptr;

    // So gun flashes light up areas.
    int extralight = 0;

    // Current PLAYPAL, ???
    //  can be set to REDCOLORMAP for pain, etc.
    int fixedcolormap = 0;

    // Player skin colorshift,
    //  0-3 for which color to draw player.
    int colormap = 0;

    // Overlay view sprites (gun, etc).
    PspDef psprites[numPSprites];

    // True if secret level has been done.
    bool didsecret = false;

    // One tic of this player (vanilla's P_PlayerThink): weapon change, movement,
    // view bob, specials and powerup countdowns. p_tick calls it; the rest are its
    // helpers. Bodies in Sim/Player.cpp. Golden-neutral, covered by every demo.
    void think();
    void thrust(Angle angle, Fixed move);
    void calcHeight();
    void movePlayer();
    void deathThink();

    // Weapon-action methods (vanilla's A_* weapon codepointers): Sim/Info.cpp's
    // state table installs each as &Player::name, and setPsprite invokes the one
    // its weapon state names via (player.*action.weapon)(psp). The bodies live in
    // Sim/Weapon.cpp, except the super-shotgun trio, which lives in Sim/Enemy.cpp.
    void weaponReady(PspDef& psp);
    void reFire(PspDef& psp);
    void checkReload(PspDef& psp);
    void lower(PspDef& psp);
    void raise(PspDef& psp);
    void gunFlash(PspDef& psp);
    void punch(PspDef& psp);
    void saw(PspDef& psp);
    void fireMissile(PspDef& psp);
    void fireBFG(PspDef& psp);
    void firePlasma(PspDef& psp);
    void firePistol(PspDef& psp);
    void fireShotgun(PspDef& psp);
    void fireShotgun2(PspDef& psp);
    void fireCGun(PspDef& psp);
    void light0(PspDef& psp);
    void light1(PspDef& psp);
    void light2(PspDef& psp);
    void bfgSound(PspDef& psp);
    void openShotgun2(PspDef& psp);
    void loadShotgun2(PspDef& psp);
    void closeShotgun2(PspDef& psp);

    // Weapon / psprite plumbing the think loop and interaction code drive directly
    // (vanilla's P_* psprite routines). Bodies in Sim/Weapon.cpp.
    void setupPsprites();
    void movePsprites();
    void bringUpWeapon();
    bool checkAmmo();
    void fireWeapon();
    void dropWeapon();
    void setPsprite(PspNum position, StateNum stnum);

    // Use the special line the player is facing (vanilla P_UseLines, body in
    // Sim/MapAction.cpp), and apply the effect of the special sector under the player
    // (vanilla P_PlayerInSpecialSector, body in Sim/Specials.cpp).
    void useLines();
    void inSpecialSector();

    // Pickups (vanilla P_Give*): grant ammo/weapon/health/armor/key/powerup, each
    // returning false if it had no effect. Bodies in Sim/Interaction.cpp.
    bool giveAmmo(AmmoType ammo, int num);
    bool giveWeapon(WeaponType weapon, bool dropped);
    bool giveBody(int num);
    bool giveArmor(int armortype);
    void giveCard(Card card);
    bool givePower(PowerType power);
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
    bool in = false; // whether the player is in game

    // Player stats, kills, collected items etc.
    int skills = 0;
    int sitems = 0;
    int ssecret = 0;
    int stime = 0;
    int frags[4] = {};
    int score = 0; // current score on entry, modified on return
};
} // namespace Doom

namespace Doom
{
struct IntermissionStart
{
    int epsd = 0; // episode # (0-2)

    // if true, splash the secret level
    bool didsecret = false;

    // previous and next levels, origin 0
    int last = 0;
    int next = 0;

    int maxkills = 0;
    int maxitems = 0;
    int maxsecret = 0;
    int maxfrags = 0;

    // the par time
    int partime = 0;

    // index of this player in game
    int pnum = 0;

    IntermissionPlayer plyr[MAXPLAYERS];
};
} // namespace Doom

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
