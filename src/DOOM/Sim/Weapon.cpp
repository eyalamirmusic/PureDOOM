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
// Rewritten into namespace Doom out of vanilla p_pspr; p_pspr.cpp keeps the vanilla
// A_*/P_* names as shims (info.cpp's states reference the A_* by address, so they
// stay global). bulletslope is file-local here now.
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

// Forward declarations so call order needs no rearranging.
void bringUpWeapon(Player& player);
bool checkAmmo(Player& player);
void fireWeapon(Player& player);
void dropWeapon(Player& player);
void computeBulletSlope(Mobj& mo);
void gunShot(Mobj& mo, bool accurate);
void setupPsprites(Player& player);
void movePsprites(Player& player);

void setPsprite(Player& player, PspNum position, StateNum stnum)
{
    PspDef* psp = &player.psprites[toIndex(position)];

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
            (player.*(state->action.weapon))(*psp);
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
void bringUpWeapon(Player& player)
{
    if (player.pendingweapon == WeaponType::NoChange)
        player.pendingweapon = player.readyweapon;

    if (player.pendingweapon == WeaponType::Chainsaw)
        startSound(player.mo, SfxEnum::Sawup);

    StateNum newstate =
        static_cast<StateNum>(weaponinfo()[toIndex(player.pendingweapon)].upstate);

    player.pendingweapon = WeaponType::NoChange;
    player.psprites[toIndex(PspNum::Weapon)].sy = WEAPONBOTTOM;

    setPsprite(player, PspNum::Weapon, newstate);
}

//
// checkAmmo
// Returns true if there is enough ammo to shoot.
// If not, selects the next weapon to use.
//
bool checkAmmo(Player& player)
{
    int count;

    AmmoType ammo = weaponinfo()[toIndex(player.readyweapon)].ammo;

    // Minimal amount for one shot varies.
    if (player.readyweapon == WeaponType::Bfg)
        count = BFGCELLS;
    else if (player.readyweapon == WeaponType::SuperShotgun)
        count = 2; // Double barrel.
    else
        count = 1; // Regular.

    // Some do not need ammunition anyway.
    // Return if current ammunition sufficient.
    if (ammo == AmmoType::NoAmmo || player.ammo[toIndex(ammo)] >= count)
        return true;

    // Out of ammo, pick a weapon to change to.
    // Preferences are set here.
    const auto& version = gameVersion();

    do
    {
        if (player.weaponowned[toIndex(WeaponType::Plasma)]
            && player.ammo[toIndex(AmmoType::Cell)]
            && (version.gamemode != GameMode::Shareware))
        {
            player.pendingweapon = WeaponType::Plasma;
        }
        else if (player.weaponowned[toIndex(WeaponType::SuperShotgun)]
                 && player.ammo[toIndex(AmmoType::Shell)] > 2
                 && (version.gamemode == GameMode::Commercial))
        {
            player.pendingweapon = WeaponType::SuperShotgun;
        }
        else if (player.weaponowned[toIndex(WeaponType::Chaingun)]
                 && player.ammo[toIndex(AmmoType::Clip)])
        {
            player.pendingweapon = WeaponType::Chaingun;
        }
        else if (player.weaponowned[toIndex(WeaponType::Shotgun)]
                 && player.ammo[toIndex(AmmoType::Shell)])
        {
            player.pendingweapon = WeaponType::Shotgun;
        }
        else if (player.ammo[toIndex(AmmoType::Clip)])
        {
            player.pendingweapon = WeaponType::Pistol;
        }
        else if (player.weaponowned[toIndex(WeaponType::Chainsaw)])
        {
            player.pendingweapon = WeaponType::Chainsaw;
        }
        else if (player.weaponowned[toIndex(WeaponType::Missile)]
                 && player.ammo[toIndex(AmmoType::Misl)])
        {
            player.pendingweapon = WeaponType::Missile;
        }
        else if (player.weaponowned[toIndex(WeaponType::Bfg)]
                 && player.ammo[toIndex(AmmoType::Cell)] > 40
                 && (version.gamemode != GameMode::Shareware))
        {
            player.pendingweapon = WeaponType::Bfg;
        }
        else
        {
            // If everything fails.
            player.pendingweapon = WeaponType::Fist;
        }

    } while (player.pendingweapon == WeaponType::NoChange);

    // Now set appropriate weapon overlay.
    setPsprite(
        player,
        PspNum::Weapon,
        static_cast<StateNum>(weaponinfo()[toIndex(player.readyweapon)].downstate));

    return false;
}

//
// fireWeapon.
//
void fireWeapon(Player& player)
{
    if (!checkAmmo(player))
        return;

    setMobjState(*player.mo, StateNum::PlayAtk1);
    StateNum newstate =
        static_cast<StateNum>(weaponinfo()[toIndex(player.readyweapon)].atkstate);
    setPsprite(player, PspNum::Weapon, newstate);
    noiseAlert(*player.mo, *player.mo);

    // [pd] Stop gun bobbing when shooting
    PspDef* psp = &player.psprites[toIndex(PspNum::Weapon)];
    psp->sx = FRACUNIT;
    psp->sy = WEAPONTOP;
}

//
// dropWeapon
// Player died, so put the weapon away.
//
void dropWeapon(Player& player)
{
    setPsprite(
        player,
        PspNum::Weapon,
        static_cast<StateNum>(weaponinfo()[toIndex(player.readyweapon)].downstate));
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
        setMobjState(*mo, StateNum::Play);
    }

    if (readyweapon == WeaponType::Chainsaw
        && psp.state == &states()[toIndex(StateNum::Saw)])
    {
        startSound(mo, SfxEnum::Sawidl);
    }

    // check for change
    //  if *this is dead, put the weapon away
    if (pendingweapon != WeaponType::NoChange || !health)
    {
        // change weapon
        //  (pending weapon should allready be validated)
        StateNum newstate =
            static_cast<StateNum>(weaponinfo()[toIndex(readyweapon)].downstate);
        setPsprite(*this, PspNum::Weapon, newstate);
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
            fireWeapon(*this);
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
        fireWeapon(*this);
    }
    else
    {
        refire = 0;
        checkAmmo(*this);
    }
}

void Player::checkReload(PspDef&)
{
    checkAmmo(*this);
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
        setPsprite(*this, PspNum::Weapon, StateNum::Null);
        return;
    }

    readyweapon = pendingweapon;

    bringUpWeapon(*this);
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

    setPsprite(*this, PspNum::Weapon, newstate);
}

//
// gunFlash
//
void Player::gunFlash(PspDef&)
{
    setMobjState(*mo, StateNum::PlayAtk2);
    setPsprite(*this,
               PspNum::Flash,
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
    lineAttack(*mo, angle, MELEERANGE, aim.slope, damage);

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
    lineAttack(*mo, angle, MELEERANGE + Fixed {1}, aim.slope, damage);

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
    spawnPlayerMissile(*mo, MobjType::Rocket);
}

//
// fireBFG
//
void Player::fireBFG(PspDef&)
{
    ammo[toIndex(weaponinfo()[toIndex(readyweapon)].ammo)] -= BFGCELLS;
    spawnPlayerMissile(*mo, MobjType::Bfg);
}

//
// firePlasma
//
void Player::firePlasma(PspDef&)
{
    ammo[toIndex(weaponinfo()[toIndex(readyweapon)].ammo)]--;

    setPsprite(
        *this,
        PspNum::Flash,
        static_cast<StateNum>(toIndex(weaponinfo()[toIndex(readyweapon)].flashstate)
                              + (randomness().forPlay() & 1)));

    spawnPlayerMissile(*mo, MobjType::Plasma);
}

//
// computeBulletSlope
// Sets a slope so a near miss is at aproximately
// the height of the intended target
//
void computeBulletSlope(Mobj& mo)
{
    auto& scratch = weaponScratch();

    // see which target is to be aimed at
    Angle an = mo.angle;
    auto aim = aimLineAttack(&mo, an, 16 * 64 * FRACUNIT);
    scratch.bulletslope = aim.slope;

    if (!aim.target)
    {
        an += Angle {1u << 26};
        aim = aimLineAttack(&mo, an, 16 * 64 * FRACUNIT);
        scratch.bulletslope = aim.slope;
        if (!aim.target)
        {
            an -= Angle {2u << 26};
            aim = aimLineAttack(&mo, an, 16 * 64 * FRACUNIT);
            scratch.bulletslope = aim.slope;
        }
    }
}

//
// gunShot
//
void gunShot(Mobj& mo, bool accurate)
{
    int damage = 5 * (randomness().forPlay() % 3 + 1);
    Angle angle = mo.angle;

    if (!accurate)
        angle += Angle {(unsigned) (randomness().forPlay() - randomness().forPlay())
                        << 18};

    lineAttack(mo, angle, MISSILERANGE, weaponScratch().bulletslope, damage);
}

//
// firePistol
//
void Player::firePistol(PspDef&)
{
    startSound(mo, SfxEnum::Pistol);

    setMobjState(*mo, StateNum::PlayAtk2);
    ammo[toIndex(weaponinfo()[toIndex(readyweapon)].ammo)]--;

    setPsprite(*this,
               PspNum::Flash,
               static_cast<StateNum>(
                   toIndex(weaponinfo()[toIndex(readyweapon)].flashstate)));

    computeBulletSlope(*mo);
    gunShot(*mo, !refire);
}

//
// fireShotgun
//
void Player::fireShotgun(PspDef&)
{
    startSound(mo, SfxEnum::Shotgn);
    setMobjState(*mo, StateNum::PlayAtk2);

    ammo[toIndex(weaponinfo()[toIndex(readyweapon)].ammo)]--;

    setPsprite(*this,
               PspNum::Flash,
               static_cast<StateNum>(
                   toIndex(weaponinfo()[toIndex(readyweapon)].flashstate)));

    computeBulletSlope(*mo);

    for (int i = 0; i < 7; i++)
        gunShot(*mo, false);
}

//
// fireShotgun2
//
void Player::fireShotgun2(PspDef&)
{
    startSound(mo, SfxEnum::Dshtgn);
    setMobjState(*mo, StateNum::PlayAtk2);

    ammo[toIndex(weaponinfo()[toIndex(readyweapon)].ammo)] -= 2;

    setPsprite(*this,
               PspNum::Flash,
               static_cast<StateNum>(
                   toIndex(weaponinfo()[toIndex(readyweapon)].flashstate)));

    computeBulletSlope(*mo);

    for (int i = 0; i < 20; i++)
    {
        int damage = 5 * (randomness().forPlay() % 3 + 1);
        Angle angle = mo->angle;
        angle += Angle {(unsigned) (randomness().forPlay() - randomness().forPlay())
                        << 19};
        lineAttack(
            *mo,
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

    setMobjState(*mo, StateNum::PlayAtk2);
    ammo[toIndex(weaponinfo()[toIndex(readyweapon)].ammo)]--;

    setPsprite(
        *this,
        PspNum::Flash,
        static_cast<StateNum>(toIndex(weaponinfo()[toIndex(readyweapon)].flashstate)
                              + psp.state - &states()[toIndex(StateNum::Chain1)]));

    computeBulletSlope(*mo);

    gunShot(*mo, !refire);
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

        damageMobj(*aim.target, target, target, damage);
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
void setupPsprites(Player& player)
{
    // remove all psprites
    for (int i = 0; i < numPSprites; i++)
        player.psprites[i].state = nullptr;

    // spawn the gun
    player.pendingweapon = player.readyweapon;
    bringUpWeapon(player);
}

//
// movePsprites
// Called every tic by player thinking routine.
//
void movePsprites(Player& player)
{
    State* state;

    PspDef* psp = &player.psprites[0];
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
                    setPsprite(
                        player, static_cast<PspNum>(i), psp->state->nextstate);
            }
        }
    }

    player.psprites[toIndex(PspNum::Flash)].sx =
        player.psprites[toIndex(PspNum::Weapon)].sx;
    player.psprites[toIndex(PspNum::Flash)].sy =
        player.psprites[toIndex(PspNum::Weapon)].sy;
}
} // namespace Doom
