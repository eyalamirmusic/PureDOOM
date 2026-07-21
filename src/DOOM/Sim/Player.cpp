// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Doom::Player related stuff: bobbing POV/weapon, movement, pending weapon.
//
// Rewritten into namespace Doom out of vanilla p_user; p_user.cpp keeps the vanilla
// name P_PlayerThink as a shim. Everything else (thrust, calcHeight, movePlayer,
// deathThink and the onground flag) is internal to this file.
//
//-----------------------------------------------------------------------------

#include "../Host/Platform.h"

#include "../Game/Event.h"
#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"
#include "SimDefs.h"

#include "../Game/GameVersion.h"
#include "../Game/LevelStats.h"
#include "Player.h"
#include "PlayerScratch.h"

// Index of the special effects (INVUL inverse) map.
#include "Specials.h"
#include "../Render/Main.h"
#include "MapAction.h"
#include "Mobj.h"
#include "Weapon.h"

namespace Doom
{

constexpr int INVERSECOLORMAP = 32;

// 16 pixels of bob (a raw fixed value: 0x100000 is 16.0)
constexpr fixed_t MAXBOB {0x100000};

constexpr angle_t ANG5 = ang90 / 18;

//
// Movement.
//

// onground now lives on the Engine (Sim/PlayerScratch.h, moved by the file-scope-statics sweep -
// REFACTOR.md, Step 5). movePlayer hoists playerScratch() once and reaches it through it; calcHeight
// and deathThink each touch it exactly once and reach it inline, rather than through a file-scope
// reference alias (REFACTOR.md, Step 9 strand (a)). Read by no other file.

//
// thrust
// Moves the given origin along a given angle.
//
void thrust(Player& player, angle_t angle, fixed_t move)
{
    const auto angleFine = angle.fineIndex();

    player.mo->momx += FixedMul(move, finecosine[angleFine]);
    player.mo->momy += FixedMul(move, finesine[angleFine]);
}

//
// calcHeight
// Calculate the walking / running height adjustment
//
void calcHeight(Player& player)
{
    // Regular movement bobbing
    // (needs to be calculated for gun swing
    // even if not on ground)
    // OPTIMIZE: tablify angle
    // Note: a LUT allows for effects
    //  like a ramp with low health.
    player.bob = FixedMul(player.mo->momx, player.mo->momx)
                 + FixedMul(player.mo->momy, player.mo->momy);

    player.bob >>= 2;

    if (player.bob > MAXBOB)
        player.bob = MAXBOB;

    if ((hasFlag(player.cheats, CheatFlag::NoMomentum)) || !playerScratch().onground)
    {
        player.viewz = player.mo->z + VIEWHEIGHT;

        if (player.viewz > player.mo->ceilingz - 4 * FRACUNIT)
            player.viewz = player.mo->ceilingz - 4 * FRACUNIT;

        player.viewz = player.mo->z + player.viewheight;
        return;
    }

    int angle = (fineAngles / 20 * levelStats().leveltime) & fineMask;
    fixed_t bob = FixedMul(player.bob / 2, finesine[angle]);

    // move viewheight
    if (player.playerstate == PlayerLifeState::Live)
    {
        player.viewheight += player.deltaviewheight;

        if (player.viewheight > VIEWHEIGHT)
        {
            player.viewheight = VIEWHEIGHT;
            player.deltaviewheight = fixed_t {};
        }

        if (player.viewheight < VIEWHEIGHT / 2)
        {
            player.viewheight = VIEWHEIGHT / 2;
            if (!player.deltaviewheight.isPositive())
                player.deltaviewheight = fixed_t {1};
        }

        if (player.deltaviewheight)
        {
            player.deltaviewheight += FRACUNIT / 4;
            if (!player.deltaviewheight)
                player.deltaviewheight = fixed_t {1};
        }
    }
    player.viewz = player.mo->z + player.viewheight + bob;

    if (player.viewz > player.mo->ceilingz - 4 * FRACUNIT)
        player.viewz = player.mo->ceilingz - 4 * FRACUNIT;
}

//
// movePlayer
//
void movePlayer(Player& player)
{
    Ticcmd* cmd = &player.cmd;

    auto& scratch = playerScratch();

    player.mo->angle += angle_t {(unsigned) cmd->angleturn << 16};

    // Do not let the player control movement
    //  if not onground.
    scratch.onground = (player.mo->z <= player.mo->floorz);

    if (cmd->forwardmove && scratch.onground)
        thrust(player, player.mo->angle, fixed_t {cmd->forwardmove * 2048});

    if (cmd->sidemove && scratch.onground)
        thrust(player, player.mo->angle - ang90, fixed_t {cmd->sidemove * 2048});

    if ((cmd->forwardmove || cmd->sidemove)
        && player.mo->state == &states[toIndex(StateNum::Play)])
    {
        setMobjState(*player.mo, StateNum::PlayRun1);
    }
}

//
// deathThink
// Fall on your face when dying.
// Decrease POV height to floor height.
//
void deathThink(Player& player)
{
    movePsprites(player);

    // fall to the ground
    if (player.viewheight > 6 * FRACUNIT)
        player.viewheight -= FRACUNIT;

    if (player.viewheight < 6 * FRACUNIT)
        player.viewheight = 6 * FRACUNIT;

    player.deltaviewheight = fixed_t {};
    playerScratch().onground = (player.mo->z <= player.mo->floorz);
    calcHeight(player);

    if (player.attacker && player.attacker != player.mo)
    {
        angle_t angle = pointToAngle2(
            player.mo->x, player.mo->y, player.attacker->x, player.attacker->y);

        angle_t delta = angle - player.mo->angle;

        if (delta < ANG5 || delta > -ANG5)
        {
            // Looking at killer,
            //  so fade damage flash down.
            player.mo->angle = angle;

            if (player.damagecount)
                player.damagecount--;
        }
        else if (delta < ang180)
            player.mo->angle += ANG5;
        else
            player.mo->angle -= ANG5;
    }
    else if (player.damagecount)
        player.damagecount--;

    if (hasFlag(player.cmd.buttons, ButtonCode::Use))
        player.playerstate = PlayerLifeState::Reborn;
}

//
// playerThink
//
void playerThink(Player& player)
{
    // fixme: do this in the cheat code
    if (hasFlag(player.cheats, CheatFlag::NoClip))
        player.mo->flags = withFlags(player.mo->flags, MobjFlag::NoClip);
    else
        player.mo->flags = withoutFlags(player.mo->flags, MobjFlag::NoClip);

    // chain saw run forward
    Ticcmd* cmd = &player.cmd;
    if (hasFlag(player.mo->flags, MobjFlag::JustAttacked))
    {
        cmd->angleturn = 0;
        cmd->forwardmove = 0xc800 / 512;
        cmd->sidemove = 0;
        player.mo->flags = withoutFlags(player.mo->flags, MobjFlag::JustAttacked);
    }

    if (player.playerstate == PlayerLifeState::Dead)
    {
        deathThink(player);
        return;
    }

    // Move around.
    // Reactiontime is used to prevent movement
    //  for a bit after a teleport.
    if (player.mo->reactiontime)
        player.mo->reactiontime--;
    else
        movePlayer(player);

    calcHeight(player);

    if (player.mo->subsector->sector->special)
        playerInSpecialSector(player);

    // Check for weapon change.

    // A special event has no other buttons.
    if (hasFlag(cmd->buttons, ButtonCode::Special))
        cmd->buttons = 0;

    if (hasFlag(cmd->buttons, ButtonCode::Change))
    {
        // The actual changing of the weapon is done
        //  when the weapon psprite can do it
        //  (read: not in the middle of an attack).
        WeaponType newweapon = static_cast<WeaponType>(
            (cmd->buttons & buttonWeaponMask) >> buttonWeaponShift);

        if (newweapon == WeaponType::Fist
            && player.weaponowned[toIndex(WeaponType::Chainsaw)]
            && !(player.readyweapon == WeaponType::Chainsaw
                 && player.powers[toIndex(PowerType::Strength)]))
        {
            newweapon = WeaponType::Chainsaw;
        }

        const auto& version = gameVersion();

        if ((version.gamemode == GameMode::Commercial)
            && newweapon == WeaponType::Shotgun
            && player.weaponowned[toIndex(WeaponType::SuperShotgun)]
            && player.readyweapon != WeaponType::SuperShotgun)
        {
            newweapon = WeaponType::SuperShotgun;
        }

        if (player.weaponowned[toIndex(newweapon)]
            && newweapon != player.readyweapon)
        {
            // Do not go to plasma or BFG in shareware,
            //  even if cheated.
            if ((newweapon != WeaponType::Plasma && newweapon != WeaponType::Bfg)
                || (version.gamemode != GameMode::Shareware))
            {
                player.pendingweapon = newweapon;
            }
        }
    }

    // check for use
    if (hasFlag(cmd->buttons, ButtonCode::Use))
    {
        if (!player.usedown)
        {
            useLines(player);
            player.usedown = true;
        }
    }
    else
        player.usedown = false;

    // cycle psprites
    movePsprites(player);

    // Counters, time dependend power ups.

    // Strength counts up to diminish fade.
    if (player.powers[toIndex(PowerType::Strength)])
        player.powers[toIndex(PowerType::Strength)]++;

    if (player.powers[toIndex(PowerType::Invulnerability)])
        player.powers[toIndex(PowerType::Invulnerability)]--;

    if (player.powers[toIndex(PowerType::Invisibility)])
        if (!--player.powers[toIndex(PowerType::Invisibility)])
            player.mo->flags = withoutFlags(player.mo->flags, MobjFlag::Shadow);

    if (player.powers[toIndex(PowerType::Infrared)])
        player.powers[toIndex(PowerType::Infrared)]--;

    if (player.powers[toIndex(PowerType::IronFeet)])
        player.powers[toIndex(PowerType::IronFeet)]--;

    if (player.damagecount)
        player.damagecount--;

    if (player.bonuscount)
        player.bonuscount--;

    // Handling colormaps.
    if (player.powers[toIndex(PowerType::Invulnerability)])
    {
        if (player.powers[toIndex(PowerType::Invulnerability)] > 4 * 32
            || (player.powers[toIndex(PowerType::Invulnerability)] & 8))
            player.fixedcolormap = INVERSECOLORMAP;
        else
            player.fixedcolormap = 0;
    }
    else if (player.powers[toIndex(PowerType::Infrared)])
    {
        if (player.powers[toIndex(PowerType::Infrared)] > 4 * 32
            || (player.powers[toIndex(PowerType::Infrared)] & 8))
        {
            // almost full bright
            player.fixedcolormap = 1;
        }
        else
            player.fixedcolormap = 0;
    }
    else
        player.fixedcolormap = 0;
}
} // namespace Doom
