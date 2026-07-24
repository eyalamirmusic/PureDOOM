// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Weapon sprite animation and the weapon action functions (A_*) that
//        info.cpp's state table drives.
//
// Rewritten into namespace Doom out of vanilla p_pspr. The A_* weapon codepointers
// are Player methods now (bfgSpray a Mobj method), installed in info.cpp's state
// table as &Player::name / &Mobj::name; the psprite plumbing and the hitscan helpers
// are methods too (see Weapon.h). bulletslope is file-local here.
//
//-----------------------------------------------------------------------------

#include "../Host/Diagnostics.h"
#include "../Host/Platform.h"

#include "../Game/Event.h"
#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"
#include "Random.h"
#include "SimDefs.h"
#include "../Game/SoundData.h"

#include "../Game/GameVersion.h"
#include "../Game/LevelStats.h"
#include "Weapon.h"
#include "WeaponScratch.h"

#include "../Game/Sound.h"
#include "../Render/Main.h"
#include "Enemy.h"
#include "Interaction.h"
#include "MapAction.h"
#include "Mobj.h"
#include "Random.h"

namespace Doom
{
constexpr Fixed LOWERSPEED = FRACUNIT * 6;
constexpr Fixed RAISESPEED = FRACUNIT * 6;

constexpr Fixed WEAPONBOTTOM = 128 * FRACUNIT;
constexpr Fixed WEAPONTOP = 32 * FRACUNIT;

// plasma cells for a bfg attack
constexpr int BFGCELLS = 40;

// The auto-aim slope of the shot being fired; file-local. The weapon scratch now lives on the
// Engine (Sim/WeaponScratch.h, moved by the file-scope-statics sweep - REFACTOR.md, Step 5).
// computeBulletSlope hoists weaponScratch() once and reaches bulletslope through it; gunShot and
// fireShotgun2 each touch it exactly once and reach it inline, rather than through file-scope
// reference aliases (REFACTOR.md, Step 9 strand (a)).

void Player::setPsprite(PspNum position, StateNum stnum)
{
    PspDef* psp = &psprites[toIndex(position)];

    do
    {
        if (stnum == StateNum::Null)
        {
            // object removed itself
            psp->state = nullptr;
            break;
        }

        State* state = &states()[toIndex(stnum)];
        psp->state = state;
        psp->tics = state->tics; // could be 0

        if (state->misc1)
        {
            // coordinate set
            psp->sx = Fixed::fromInt(state->misc1);
            psp->sy = Fixed::fromInt(state->misc2);
        }

        // Call action routine.
        if (state->action.weapon)
        {
            (this->*(state->action.weapon))(*psp);
            if (!psp->state)
                break;
        }

        stnum = psp->state->nextstate;

    } while (!psp->tics);
    // an initial state of 0 could cycle through
}

//
// bringUpWeapon
// Starts bringing the pending weapon up
// from the bottom of the screen.
// Uses player
//
void Player::bringUpWeapon()
{
    if (pendingweapon == WeaponType::NoChange)
        pendingweapon = readyweapon;

    if (pendingweapon == WeaponType::Chainsaw)
        startSound(mo, SfxEnum::Sawup);

    StateNum newstate =
        static_cast<StateNum>(weaponinfo()[toIndex(pendingweapon)].upstate);

    pendingweapon = WeaponType::NoChange;
    psprites[toIndex(PspNum::Weapon)].sy = WEAPONBOTTOM;

    setPsprite(PspNum::Weapon, newstate);
}

//
// checkAmmo
// Returns true if there is enough ammo to shoot.
// If not, selects the next weapon to use.
//
bool Player::checkAmmo()
{
    int count;

    AmmoType ammoType = weaponinfo()[toIndex(readyweapon)].ammo;

    // Minimal amount for one shot varies.
    if (readyweapon == WeaponType::Bfg)
        count = BFGCELLS;
    else if (readyweapon == WeaponType::SuperShotgun)
        count = 2; // Double barrel.
    else
        count = 1; // Regular.

    // Some do not need ammunition anyway.
    // Return if current ammunition sufficient.
    if (ammoType == AmmoType::NoAmmo || ammo[toIndex(ammoType)] >= count)
        return true;

    // Out of ammo, pick a weapon to change to.
    // Preferences are set here.
    const auto& version = gameVersion();

    do
    {
        if (weaponowned[toIndex(WeaponType::Plasma)] && ammo[toIndex(AmmoType::Cell)]
            && (version.gamemode != GameMode::Shareware))
        {
            pendingweapon = WeaponType::Plasma;
        }
        else if (weaponowned[toIndex(WeaponType::SuperShotgun)]
                 && ammo[toIndex(AmmoType::Shell)] > 2
                 && (version.gamemode == GameMode::Commercial))
        {
            pendingweapon = WeaponType::SuperShotgun;
        }
        else if (weaponowned[toIndex(WeaponType::Chaingun)]
                 && ammo[toIndex(AmmoType::Clip)])
        {
            pendingweapon = WeaponType::Chaingun;
        }
        else if (weaponowned[toIndex(WeaponType::Shotgun)]
                 && ammo[toIndex(AmmoType::Shell)])
        {
            pendingweapon = WeaponType::Shotgun;
        }
        else if (ammo[toIndex(AmmoType::Clip)])
        {
            pendingweapon = WeaponType::Pistol;
        }
        else if (weaponowned[toIndex(WeaponType::Chainsaw)])
        {
            pendingweapon = WeaponType::Chainsaw;
        }
        else if (weaponowned[toIndex(WeaponType::Missile)]
                 && ammo[toIndex(AmmoType::Misl)])
        {
            pendingweapon = WeaponType::Missile;
        }
        else if (weaponowned[toIndex(WeaponType::Bfg)]
                 && ammo[toIndex(AmmoType::Cell)] > 40
                 && (version.gamemode != GameMode::Shareware))
        {
            pendingweapon = WeaponType::Bfg;
        }
        else
        {
            // If everything fails.
            pendingweapon = WeaponType::Fist;
        }

    } while (pendingweapon == WeaponType::NoChange);

    // Now set appropriate weapon overlay.
    setPsprite(PspNum::Weapon,
               static_cast<StateNum>(weaponinfo()[toIndex(readyweapon)].downstate));

    return false;
}

//
// fireWeapon.
//
void Player::fireWeapon()
{
    if (!checkAmmo())
        return;

    mo->setState(StateNum::PlayAtk1);
    StateNum newstate =
        static_cast<StateNum>(weaponinfo()[toIndex(readyweapon)].atkstate);
    setPsprite(PspNum::Weapon, newstate);
    noiseAlert(*mo, *mo);

    // [pd] Stop gun bobbing when shooting
    PspDef* psp = &psprites[toIndex(PspNum::Weapon)];
    psp->sx = FRACUNIT;
    psp->sy = WEAPONTOP;
}

//
// dropWeapon
// Player died, so put the weapon away.
//
void Player::dropWeapon()
{
    setPsprite(PspNum::Weapon,
               static_cast<StateNum>(weaponinfo()[toIndex(readyweapon)].downstate));
}

//
// weaponReady
// The player can fire the weapon
// or change to another weapon at this time.
// Follows after getting weapon up,
// or after previous attack/fire sequence.
//
void Player::weaponReady(PspDef& psp)
{
    // get out of attack state
    if (mo->state == &states()[toIndex(StateNum::PlayAtk1)]
        || mo->state == &states()[toIndex(StateNum::PlayAtk2)])
    {
        mo->setState(StateNum::Play);
    }

    if (readyweapon == WeaponType::Chainsaw
        && psp.state == &states()[toIndex(StateNum::Saw)])
    {
        startSound(mo, SfxEnum::Sawidl);
    }

    // check for change
    //  if the player is dead, put the weapon away
    if (pendingweapon != WeaponType::NoChange || !health)
    {
        // change weapon
        //  (pending weapon should allready be validated)
        StateNum newstate =
            static_cast<StateNum>(weaponinfo()[toIndex(readyweapon)].downstate);
        setPsprite(PspNum::Weapon, newstate);
        return;
    }

    // check for fire
    //  the missile launcher and bfg do not auto fire
    if (hasFlag(cmd.buttons, ButtonCode::Attack))
    {
        if (!attackdown
            || (readyweapon != WeaponType::Missile
                && readyweapon != WeaponType::Bfg))
        {
            attackdown = true;
            fireWeapon();
            return;
        }
    }
    else
        attackdown = false;

    // bob the weapon based on movement speed
    int angle = (128 * levelStats().leveltime) & fineMask;
    psp.sx = FRACUNIT + FixedMul(bob, finecosine()[angle]);
    angle &= fineAngles / 2 - 1;
    psp.sy = WEAPONTOP + FixedMul(bob, finesine()[angle]);
}

//
// reFire
// The player can re-fire the weapon
// without lowering it entirely.
//
void Player::reFire(PspDef&)
{
    // check for fire
    //  (if a weaponchange is pending, let it go through instead)
    if ((hasFlag(cmd.buttons, ButtonCode::Attack))
        && pendingweapon == WeaponType::NoChange && health)
    {
        refire++;
        fireWeapon();
    }
    else
    {
        refire = 0;
        checkAmmo();
    }
}

void Player::checkReload(PspDef&)
{
    checkAmmo();
}

//
// lower
// Lowers current weapon,
//  and changes weapon at bottom.
//
void Player::lower(PspDef& psp)
{
    psp.sy += LOWERSPEED;

    // Is already down.
    if (psp.sy < WEAPONBOTTOM)
        return;

    // Player is dead.
    if (playerstate == PlayerLifeState::Dead)
    {
        psp.sy = WEAPONBOTTOM;

        // don't bring weapon back up
        return;
    }

    // The old weapon has been lowered off the screen,
    // so change the weapon and start raising it
    if (!health)
    {
        // Player is dead, so keep the weapon off screen.
        setPsprite(PspNum::Weapon, StateNum::Null);
        return;
    }

    readyweapon = pendingweapon;

    bringUpWeapon();
}

//
// raise
//
void Player::raise(PspDef& psp)
{
    psp.sy -= RAISESPEED;

    if (psp.sy > WEAPONTOP)
        return;

    psp.sy = WEAPONTOP;

    // The weapon has been raised all the way,
    //  so change to the ready state.
    StateNum newstate =
        static_cast<StateNum>(weaponinfo()[toIndex(readyweapon)].readystate);

    setPsprite(PspNum::Weapon, newstate);
}

//
// gunFlash
//
void Player::gunFlash(PspDef&)
{
    mo->setState(StateNum::PlayAtk2);
    setPsprite(PspNum::Flash,
               static_cast<StateNum>(
                   toIndex(weaponinfo()[toIndex(readyweapon)].flashstate)));
}

//
// WEAPON ATTACKS
//

//
// punch
//
void Player::punch(PspDef&)
{
    int damage = (randomness().forPlay() % 10 + 1) << 1;

    if (powers[toIndex(PowerType::Strength)])
        damage *= 10;

    Angle angle = mo->angle;
    angle +=
        Angle {(unsigned) (randomness().forPlay() - randomness().forPlay()) << 18};
    const auto aim = aimLineAttack(mo, angle, MELEERANGE);
    mo->lineAttack(angle, MELEERANGE, aim.slope, damage);

    // turn to face target
    if (aim.target)
    {
        startSound(mo, SfxEnum::Punch);
        mo->angle = pointToAngle2(mo->x, mo->y, aim.target->x, aim.target->y);
    }
}

//
// saw
//
void Player::saw(PspDef&)
{
    int damage = 2 * (randomness().forPlay() % 10 + 1);
    Angle angle = mo->angle;
    angle +=
        Angle {(unsigned) (randomness().forPlay() - randomness().forPlay()) << 18};

    // use meleerange + 1 se the puff doesn't skip the flash
    const auto aim = aimLineAttack(mo, angle, MELEERANGE + Fixed {1});
    mo->lineAttack(angle, MELEERANGE + Fixed {1}, aim.slope, damage);

    if (!aim.target)
    {
        startSound(mo, SfxEnum::Sawful);
        return;
    }
    startSound(mo, SfxEnum::Sawhit);

    // turn to face target
    angle = pointToAngle2(mo->x, mo->y, aim.target->x, aim.target->y);
    if (angle - mo->angle > ang180)
    {
        if (angle - mo->angle < static_cast<Angle>(-ang90 / 20))
            mo->angle = angle + ang90 / 21;
        else
            mo->angle -= ang90 / 20;
    }
    else
    {
        if (angle - mo->angle > ang90 / 20)
            mo->angle = angle - ang90 / 21;
        else
            mo->angle += ang90 / 20;
    }
    mo->flags = withFlags(mo->flags, MobjFlag::JustAttacked);
}

//
// fireMissile
//
void Player::fireMissile(PspDef&)
{
    ammo[toIndex(weaponinfo()[toIndex(readyweapon)].ammo)]--;
    mo->spawnPlayerMissile(MobjType::Rocket);
}

//
// fireBFG
//
void Player::fireBFG(PspDef&)
{
    ammo[toIndex(weaponinfo()[toIndex(readyweapon)].ammo)] -= BFGCELLS;
    mo->spawnPlayerMissile(MobjType::Bfg);
}

//
// firePlasma
//
void Player::firePlasma(PspDef&)
{
    ammo[toIndex(weaponinfo()[toIndex(readyweapon)].ammo)]--;

    setPsprite(
        PspNum::Flash,
        static_cast<StateNum>(toIndex(weaponinfo()[toIndex(readyweapon)].flashstate)
                              + (randomness().forPlay() & 1)));

    mo->spawnPlayerMissile(MobjType::Plasma);
}

//
// computeBulletSlope
// Sets a slope so a near miss is at aproximately
// the height of the intended target
//
void Mobj::computeBulletSlope()
{
    auto& scratch = weaponScratch();

    // see which target is to be aimed at
    Angle an = angle;
    auto aim = aimLineAttack(this, an, 16 * 64 * FRACUNIT);
    scratch.bulletslope = aim.slope;

    if (!aim.target)
    {
        an += Angle {1u << 26};
        aim = aimLineAttack(this, an, 16 * 64 * FRACUNIT);
        scratch.bulletslope = aim.slope;
        if (!aim.target)
        {
            an -= Angle {2u << 26};
            aim = aimLineAttack(this, an, 16 * 64 * FRACUNIT);
            scratch.bulletslope = aim.slope;
        }
    }
}

//
// gunShot
//
void Mobj::gunShot(bool accurate)
{
    int damage = 5 * (randomness().forPlay() % 3 + 1);
    Angle angleToUse = angle;

    if (!accurate)
        angleToUse += Angle {
            (unsigned) (randomness().forPlay() - randomness().forPlay()) << 18};

    lineAttack(angleToUse, MISSILERANGE, weaponScratch().bulletslope, damage);
}

//
// firePistol
//
void Player::firePistol(PspDef&)
{
    startSound(mo, SfxEnum::Pistol);

    mo->setState(StateNum::PlayAtk2);
    ammo[toIndex(weaponinfo()[toIndex(readyweapon)].ammo)]--;

    setPsprite(PspNum::Flash,
               static_cast<StateNum>(
                   toIndex(weaponinfo()[toIndex(readyweapon)].flashstate)));

    mo->computeBulletSlope();
    mo->gunShot(!refire);
}

//
// fireShotgun
//
void Player::fireShotgun(PspDef&)
{
    startSound(mo, SfxEnum::Shotgn);
    mo->setState(StateNum::PlayAtk2);

    ammo[toIndex(weaponinfo()[toIndex(readyweapon)].ammo)]--;

    setPsprite(PspNum::Flash,
               static_cast<StateNum>(
                   toIndex(weaponinfo()[toIndex(readyweapon)].flashstate)));

    mo->computeBulletSlope();

    for (int i = 0; i < 7; i++)
        mo->gunShot(false);
}

//
// fireShotgun2
//
void Player::fireShotgun2(PspDef&)
{
    startSound(mo, SfxEnum::Dshtgn);
    mo->setState(StateNum::PlayAtk2);

    ammo[toIndex(weaponinfo()[toIndex(readyweapon)].ammo)] -= 2;

    setPsprite(PspNum::Flash,
               static_cast<StateNum>(
                   toIndex(weaponinfo()[toIndex(readyweapon)].flashstate)));

    mo->computeBulletSlope();

    for (int i = 0; i < 20; i++)
    {
        int damage = 5 * (randomness().forPlay() % 3 + 1);
        Angle angle = mo->angle;
        angle += Angle {(unsigned) (randomness().forPlay() - randomness().forPlay())
                        << 19};
        mo->lineAttack(
            angle,
            MISSILERANGE,
            weaponScratch().bulletslope
                + Fixed {(randomness().forPlay() - randomness().forPlay()) << 5},
            damage);
    }
}

//
// fireCGun
//
void Player::fireCGun(PspDef& psp)
{
    startSound(mo, SfxEnum::Pistol);

    if (!ammo[toIndex(weaponinfo()[toIndex(readyweapon)].ammo)])
        return;

    mo->setState(StateNum::PlayAtk2);
    ammo[toIndex(weaponinfo()[toIndex(readyweapon)].ammo)]--;

    setPsprite(
        PspNum::Flash,
        static_cast<StateNum>(toIndex(weaponinfo()[toIndex(readyweapon)].flashstate)
                              + psp.state - &states()[toIndex(StateNum::Chain1)]));

    mo->computeBulletSlope();

    mo->gunShot(!refire);
}

//
// ?
//
void Player::light0(PspDef&)
{
    extralight = 0;
}

void Player::light1(PspDef&)
{
    extralight = 1;
}

void Player::light2(PspDef&)
{
    extralight = 2;
}

//
// bfgSpray
// Spawn a BFG explosion on every monster in view
//
void Mobj::bfgSpray()
{
    // offset angles from its attack angle
    for (int i = 0; i < 40; i++)
    {
        Angle an = angle - ang90 / 2 + ang90 / 40 * i;

        // target is the originator (player)
        //  of the missile
        const auto aim = aimLineAttack(target, an, 16 * 64 * FRACUNIT);

        if (!aim.target)
            continue;

        spawnMobj(aim.target->x,
                  aim.target->y,
                  aim.target->z + (aim.target->height >> 2),
                  MobjType::Extrabfg);

        int damage = 0;
        for (int j = 0; j < 15; j++)
            damage += (randomness().forPlay() & 7) + 1;

        aim.target->damage(target, target, damage);
    }
}

//
// bfgSound
//
void Player::bfgSound(PspDef&)
{
    startSound(mo, SfxEnum::Bfg);
}

//
// setupPsprites
// Called at start of level for each player.
//
void Player::setupPsprites()
{
    // remove all psprites
    for (int i = 0; i < numPSprites; i++)
        psprites[i].state = nullptr;

    // spawn the gun
    pendingweapon = readyweapon;
    bringUpWeapon();
}

//
// movePsprites
// Called every tic by player thinking routine.
//
void Player::movePsprites()
{
    State* state;

    PspDef* psp = &psprites[0];
    for (int i = 0; i < numPSprites; i++, psp++)
    {
        // a null state means not active
        if ((state = psp->state))
        {
            // drop tic count and possibly change state

            // a -1 tic count never changes
            if (psp->tics != -1)
            {
                psp->tics--;
                if (!psp->tics)
                    setPsprite(static_cast<PspNum>(i), psp->state->nextstate);
            }
        }
    }

    psprites[toIndex(PspNum::Flash)].sx = psprites[toIndex(PspNum::Weapon)].sx;
    psprites[toIndex(PspNum::Flash)].sy = psprites[toIndex(PspNum::Weapon)].sy;
}
} // namespace Doom
