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
// Rewritten into namespace Doom out of vanilla p_user. One tic of a player is
// Player::think() (vanilla's P_PlayerThink), with thrust / calcHeight / movePlayer /
// deathThink its helper methods - all declared on the struct in Game/PlayerTypes.h,
// bodies here. The onground flag lives on the Engine (Sim/PlayerScratch.h).
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
constexpr Fixed MAXBOB {0x100000};

constexpr Angle ANG5 = ang90 / 18;

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
void Player::thrust(Angle angle, Fixed move)
{
    const auto angleFine = angle.fineIndex();

    mo->momx += FixedMul(move, finecosine()[angleFine]);
    mo->momy += FixedMul(move, finesine()[angleFine]);
}

//
// calcHeight
// Calculate the walking / running height adjustment
//
void Player::calcHeight()
{
    // Regular movement bobbing
    // (needs to be calculated for gun swing
    // even if not on ground)
    // OPTIMIZE: tablify angle
    // Note: a LUT allows for effects
    //  like a ramp with low health.
    bob = FixedMul(mo->momx, mo->momx) + FixedMul(mo->momy, mo->momy);

    bob >>= 2;

    if (bob > MAXBOB)
        bob = MAXBOB;

    if ((hasFlag(cheats, CheatFlag::NoMomentum)) || !playerScratch().onground)
    {
        viewz = mo->z + VIEWHEIGHT;

        if (viewz > mo->ceilingz - 4 * FRACUNIT)
            viewz = mo->ceilingz - 4 * FRACUNIT;

        viewz = mo->z + viewheight;
        return;
    }

    int angle = (fineAngles / 20 * levelStats().leveltime) & fineMask;
    Fixed bobToUse = FixedMul(bob / 2, finesine()[angle]);

    // move viewheight
    if (playerstate == PlayerLifeState::Live)
    {
        viewheight += deltaviewheight;

        if (viewheight > VIEWHEIGHT)
        {
            viewheight = VIEWHEIGHT;
            deltaviewheight = Fixed {};
        }

        if (viewheight < VIEWHEIGHT / 2)
        {
            viewheight = VIEWHEIGHT / 2;
            if (!deltaviewheight.isPositive())
                deltaviewheight = Fixed {1};
        }

        if (deltaviewheight)
        {
            deltaviewheight += FRACUNIT / 4;
            if (!deltaviewheight)
                deltaviewheight = Fixed {1};
        }
    }
    viewz = mo->z + viewheight + bobToUse;

    if (viewz > mo->ceilingz - 4 * FRACUNIT)
        viewz = mo->ceilingz - 4 * FRACUNIT;
}

//
// movePlayer
//
void Player::movePlayer()
{
    Ticcmd* cmdToUse = &cmd;

    auto& scratch = playerScratch();

    mo->angle += Angle {(unsigned) cmdToUse->angleturn << 16};

    // Do not let the *this control movement
    //  if not onground.
    scratch.onground = (mo->z <= mo->floorz);

    if (cmdToUse->forwardmove && scratch.onground)
        thrust(mo->angle, Fixed {cmdToUse->forwardmove * 2048});

    if (cmdToUse->sidemove && scratch.onground)
        thrust(mo->angle - ang90, Fixed {cmdToUse->sidemove * 2048});

    if ((cmdToUse->forwardmove || cmdToUse->sidemove)
        && mo->state == &states()[toIndex(StateNum::Play)])
    {
        setMobjState(*mo, StateNum::PlayRun1);
    }
}

//
// deathThink
// Fall on your face when dying.
// Decrease POV height to floor height.
//
void Player::deathThink()
{
    movePsprites(*this);

    // fall to the ground
    if (viewheight > 6 * FRACUNIT)
        viewheight -= FRACUNIT;

    if (viewheight < 6 * FRACUNIT)
        viewheight = 6 * FRACUNIT;

    deltaviewheight = Fixed {};
    playerScratch().onground = (mo->z <= mo->floorz);
    calcHeight();

    if (attacker && attacker != mo)
    {
        Angle angle = pointToAngle2(mo->x, mo->y, attacker->x, attacker->y);

        Angle delta = angle - mo->angle;

        if (delta < ANG5 || delta > -ANG5)
        {
            // Looking at killer,
            //  so fade damage flash down.
            mo->angle = angle;

            if (damagecount)
                damagecount--;
        }
        else if (delta < ang180)
            mo->angle += ANG5;
        else
            mo->angle -= ANG5;
    }
    else if (damagecount)
        damagecount--;

    if (hasFlag(cmd.buttons, ButtonCode::Use))
        playerstate = PlayerLifeState::Reborn;
}

//
// playerThink
//
void Player::think()
{
    // fixme: do this in the cheat code
    if (hasFlag(cheats, CheatFlag::NoClip))
        mo->flags = withFlags(mo->flags, MobjFlag::NoClip);
    else
        mo->flags = withoutFlags(mo->flags, MobjFlag::NoClip);

    // chain saw run forward
    Ticcmd* cmdToUse = &cmd;
    if (hasFlag(mo->flags, MobjFlag::JustAttacked))
    {
        cmdToUse->angleturn = 0;
        cmdToUse->forwardmove = 0xc800 / 512;
        cmdToUse->sidemove = 0;
        mo->flags = withoutFlags(mo->flags, MobjFlag::JustAttacked);
    }

    if (playerstate == PlayerLifeState::Dead)
    {
        deathThink();
        return;
    }

    // Move around.
    // Reactiontime is used to prevent movement
    //  for a bit after a teleport.
    if (mo->reactiontime)
        mo->reactiontime--;
    else
        movePlayer();

    calcHeight();

    if (mo->subsector->sector->special)
        playerInSpecialSector(*this);

    // Check for weapon change.

    // A special event has no other buttons.
    if (hasFlag(cmdToUse->buttons, ButtonCode::Special))
        cmdToUse->buttons = 0;

    if (hasFlag(cmdToUse->buttons, ButtonCode::Change))
    {
        // The actual changing of the weapon is done
        //  when the weapon psprite can do it
        //  (read: not in the middle of an attack).
        WeaponType newweapon = static_cast<WeaponType>(
            (cmdToUse->buttons & buttonWeaponMask) >> buttonWeaponShift);

        if (newweapon == WeaponType::Fist
            && weaponowned[toIndex(WeaponType::Chainsaw)]
            && !(readyweapon == WeaponType::Chainsaw
                 && powers[toIndex(PowerType::Strength)]))
        {
            newweapon = WeaponType::Chainsaw;
        }

        const auto& version = gameVersion();

        if ((version.gamemode == GameMode::Commercial)
            && newweapon == WeaponType::Shotgun
            && weaponowned[toIndex(WeaponType::SuperShotgun)]
            && readyweapon != WeaponType::SuperShotgun)
        {
            newweapon = WeaponType::SuperShotgun;
        }

        if (weaponowned[toIndex(newweapon)] && newweapon != readyweapon)
        {
            // Do not go to plasma or BFG in shareware,
            //  even if cheated.
            if ((newweapon != WeaponType::Plasma && newweapon != WeaponType::Bfg)
                || (version.gamemode != GameMode::Shareware))
            {
                pendingweapon = newweapon;
            }
        }
    }

    // check for use
    if (hasFlag(cmdToUse->buttons, ButtonCode::Use))
    {
        if (!usedown)
        {
            useLines(*this);
            usedown = true;
        }
    }
    else
        usedown = false;

    // cycle psprites
    movePsprites(*this);

    // Counters, time dependend power ups.

    // Strength counts up to diminish fade.
    if (powers[toIndex(PowerType::Strength)])
        powers[toIndex(PowerType::Strength)]++;

    if (powers[toIndex(PowerType::Invulnerability)])
        powers[toIndex(PowerType::Invulnerability)]--;

    if (powers[toIndex(PowerType::Invisibility)])
        if (!--powers[toIndex(PowerType::Invisibility)])
            mo->flags = withoutFlags(mo->flags, MobjFlag::Shadow);

    if (powers[toIndex(PowerType::Infrared)])
        powers[toIndex(PowerType::Infrared)]--;

    if (powers[toIndex(PowerType::IronFeet)])
        powers[toIndex(PowerType::IronFeet)]--;

    if (damagecount)
        damagecount--;

    if (bonuscount)
        bonuscount--;

    // Handling colormaps.
    if (powers[toIndex(PowerType::Invulnerability)])
    {
        if (powers[toIndex(PowerType::Invulnerability)] > 4 * 32
            || (powers[toIndex(PowerType::Invulnerability)] & 8))
            fixedcolormap = INVERSECOLORMAP;
        else
            fixedcolormap = 0;
    }
    else if (powers[toIndex(PowerType::Infrared)])
    {
        if (powers[toIndex(PowerType::Infrared)] > 4 * 32
            || (powers[toIndex(PowerType::Infrared)] & 8))
        {
            // almost full bright
            fixedcolormap = 1;
        }
        else
            fixedcolormap = 0;
    }
    else
        fixedcolormap = 0;
}
} // namespace Doom
