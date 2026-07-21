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
namespace
{
// Sim/Info.cpp's states[] stores every action under one pointer shape
// (actionf_p1, void(*)(void*)), because a single table holds actions of three
// different signatures. A weapon state's action really is a (Player*, PspDef*)
// function, so casting the erased pointer back to that exact signature before
// calling it is a round trip and therefore well-defined - but neither compiler
// can see the round trip, and both object to the cast on arity alone. (The
// sibling cast in Sim/Mobj.cpp draws no warning: void(*)(Mobj*) keeps the one
// parameter, so only this two-parameter one differs in shape.)
//
// The suppression is scoped to this one function, and its spelling lives in
// Host/Diagnostics.h: the compilers disagree on the flag's name, naming the
// wrong one is itself a warning (GCC raises -Wpragmas for Clang's spelling,
// which is how three stale suppressions in this tree were found), and MSVC
// understands none of the spellings at all.
DOOM_DIAGNOSTIC_PUSH
DOOM_IGNORE_CAST_FUNCTION_TYPE

void callWeaponAction(actionf_p1 action, Player* player, PspDef* psp)
{
    reinterpret_cast<void (*)(Player*, PspDef*)>(action)(player, psp);
}

DOOM_DIAGNOSTIC_POP
} // namespace

constexpr fixed_t LOWERSPEED = FRACUNIT * 6;
constexpr fixed_t RAISESPEED = FRACUNIT * 6;

constexpr fixed_t WEAPONBOTTOM = 128 * FRACUNIT;
constexpr fixed_t WEAPONTOP = 32 * FRACUNIT;

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
void weaponReady(Player& player, PspDef& psp);
void reFire(Player& player, PspDef& psp);
void checkReload(Player& player, PspDef& psp);
void lower(Player& player, PspDef& psp);
void raise(Player& player, PspDef& psp);
void gunFlash(Player& player, PspDef& psp);
void punch(Player& player, PspDef& psp);
void saw(Player& player, PspDef& psp);
void fireMissile(Player& player, PspDef& psp);
void fireBFG(Player& player, PspDef& psp);
void firePlasma(Player& player, PspDef& psp);
void computeBulletSlope(Mobj& mo);
void gunShot(Mobj& mo, bool accurate);
void firePistol(Player& player, PspDef& psp);
void fireShotgun(Player& player, PspDef& psp);
void fireShotgun2(Player& player, PspDef& psp);
void fireCGun(Player& player, PspDef& psp);
void light0(Player& player, PspDef& psp);
void light1(Player& player, PspDef& psp);
void light2(Player& player, PspDef& psp);
void bfgSpray(Mobj& mo);
void bfgSound(Player& player, PspDef& psp);
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

        State* state = &states[toIndex(stnum)];
        psp->state = state;
        psp->tics = state->tics; // could be 0

        if (state->misc1)
        {
            // coordinate set
            psp->sx = Fixed::fromInt(state->misc1);
            psp->sy = Fixed::fromInt(state->misc2);
        }

        // Call action routine. The action is stored type-erased; a weapon state
        // carries a (Player*, PspDef*) action, cast back from the erased pointer
        // here (a round-trip conversion, hence well-defined).
        if (state->action.fn)
        {
            callWeaponAction(state->action.fn, &player, psp);
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
        static_cast<StateNum>(weaponinfo[toIndex(player.pendingweapon)].upstate);

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

    AmmoType ammo = weaponinfo[toIndex(player.readyweapon)].ammo;

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
        static_cast<StateNum>(weaponinfo[toIndex(player.readyweapon)].downstate));

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
        static_cast<StateNum>(weaponinfo[toIndex(player.readyweapon)].atkstate);
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
        static_cast<StateNum>(weaponinfo[toIndex(player.readyweapon)].downstate));
}

//
// weaponReady
// The player can fire the weapon
// or change to another weapon at this time.
// Follows after getting weapon up,
// or after previous attack/fire sequence.
//
void weaponReady(Player& player, PspDef& psp)
{
    // get out of attack state
    if (player.mo->state == &states[toIndex(StateNum::PlayAtk1)]
        || player.mo->state == &states[toIndex(StateNum::PlayAtk2)])
    {
        setMobjState(*player.mo, StateNum::Play);
    }

    if (player.readyweapon == WeaponType::Chainsaw
        && psp.state == &states[toIndex(StateNum::Saw)])
    {
        startSound(player.mo, SfxEnum::Sawidl);
    }

    // check for change
    //  if player is dead, put the weapon away
    if (player.pendingweapon != WeaponType::NoChange || !player.health)
    {
        // change weapon
        //  (pending weapon should allready be validated)
        StateNum newstate =
            static_cast<StateNum>(weaponinfo[toIndex(player.readyweapon)].downstate);
        setPsprite(player, PspNum::Weapon, newstate);
        return;
    }

    // check for fire
    //  the missile launcher and bfg do not auto fire
    if (player.cmd.buttons & BT_ATTACK)
    {
        if (!player.attackdown
            || (player.readyweapon != WeaponType::Missile
                && player.readyweapon != WeaponType::Bfg))
        {
            player.attackdown = true;
            fireWeapon(player);
            return;
        }
    }
    else
        player.attackdown = false;

    // bob the weapon based on movement speed
    int angle = (128 * levelStats().leveltime) & fineMask;
    psp.sx = FRACUNIT + FixedMul(player.bob, finecosine[angle]);
    angle &= fineAngles / 2 - 1;
    psp.sy = WEAPONTOP + FixedMul(player.bob, finesine[angle]);
}

//
// reFire
// The player can re-fire the weapon
// without lowering it entirely.
//
void reFire(Player& player, PspDef&)
{
    // check for fire
    //  (if a weaponchange is pending, let it go through instead)
    if ((player.cmd.buttons & BT_ATTACK)
        && player.pendingweapon == WeaponType::NoChange && player.health)
    {
        player.refire++;
        fireWeapon(player);
    }
    else
    {
        player.refire = 0;
        checkAmmo(player);
    }
}

void checkReload(Player& player, PspDef&)
{
    checkAmmo(player);
}

//
// lower
// Lowers current weapon,
//  and changes weapon at bottom.
//
void lower(Player& player, PspDef& psp)
{
    psp.sy += LOWERSPEED;

    // Is already down.
    if (psp.sy < WEAPONBOTTOM)
        return;

    // Player is dead.
    if (player.playerstate == PlayerLifeState::Dead)
    {
        psp.sy = WEAPONBOTTOM;

        // don't bring weapon back up
        return;
    }

    // The old weapon has been lowered off the screen,
    // so change the weapon and start raising it
    if (!player.health)
    {
        // Player is dead, so keep the weapon off screen.
        setPsprite(player, PspNum::Weapon, StateNum::Null);
        return;
    }

    player.readyweapon = player.pendingweapon;

    bringUpWeapon(player);
}

//
// raise
//
void raise(Player& player, PspDef& psp)
{
    psp.sy -= RAISESPEED;

    if (psp.sy > WEAPONTOP)
        return;

    psp.sy = WEAPONTOP;

    // The weapon has been raised all the way,
    //  so change to the ready state.
    StateNum newstate =
        static_cast<StateNum>(weaponinfo[toIndex(player.readyweapon)].readystate);

    setPsprite(player, PspNum::Weapon, newstate);
}

//
// gunFlash
//
void gunFlash(Player& player, PspDef&)
{
    setMobjState(*player.mo, StateNum::PlayAtk2);
    setPsprite(player,
               PspNum::Flash,
               static_cast<StateNum>(
                   toIndex(weaponinfo[toIndex(player.readyweapon)].flashstate)));
}

//
// WEAPON ATTACKS
//

//
// punch
//
void punch(Player& player, PspDef&)
{
    int damage = (randomness().forPlay() % 10 + 1) << 1;

    if (player.powers[toIndex(PowerType::Strength)])
        damage *= 10;

    angle_t angle = player.mo->angle;
    angle +=
        angle_t {(unsigned) (randomness().forPlay() - randomness().forPlay()) << 18};
    const auto aim = aimLineAttack(player.mo, angle, MELEERANGE);
    lineAttack(*player.mo, angle, MELEERANGE, aim.slope, damage);

    // turn to face target
    if (aim.target)
    {
        startSound(player.mo, SfxEnum::Punch);
        player.mo->angle =
            pointToAngle2(player.mo->x, player.mo->y, aim.target->x, aim.target->y);
    }
}

//
// saw
//
void saw(Player& player, PspDef&)
{
    int damage = 2 * (randomness().forPlay() % 10 + 1);
    angle_t angle = player.mo->angle;
    angle +=
        angle_t {(unsigned) (randomness().forPlay() - randomness().forPlay()) << 18};

    // use meleerange + 1 se the puff doesn't skip the flash
    const auto aim = aimLineAttack(player.mo, angle, MELEERANGE + fixed_t {1});
    lineAttack(*player.mo, angle, MELEERANGE + fixed_t {1}, aim.slope, damage);

    if (!aim.target)
    {
        startSound(player.mo, SfxEnum::Sawful);
        return;
    }
    startSound(player.mo, SfxEnum::Sawhit);

    // turn to face target
    angle = pointToAngle2(player.mo->x, player.mo->y, aim.target->x, aim.target->y);
    if (angle - player.mo->angle > ang180)
    {
        if (angle - player.mo->angle < static_cast<angle_t>(-ang90 / 20))
            player.mo->angle = angle + ang90 / 21;
        else
            player.mo->angle -= ang90 / 20;
    }
    else
    {
        if (angle - player.mo->angle > ang90 / 20)
            player.mo->angle = angle - ang90 / 21;
        else
            player.mo->angle += ang90 / 20;
    }
    player.mo->flags |= MF_JUSTATTACKED;
}

//
// fireMissile
//
void fireMissile(Player& player, PspDef&)
{
    player.ammo[toIndex(weaponinfo[toIndex(player.readyweapon)].ammo)]--;
    spawnPlayerMissile(*player.mo, MobjType::Rocket);
}

//
// fireBFG
//
void fireBFG(Player& player, PspDef&)
{
    player.ammo[toIndex(weaponinfo[toIndex(player.readyweapon)].ammo)] -= BFGCELLS;
    spawnPlayerMissile(*player.mo, MobjType::Bfg);
}

//
// firePlasma
//
void firePlasma(Player& player, PspDef&)
{
    player.ammo[toIndex(weaponinfo[toIndex(player.readyweapon)].ammo)]--;

    setPsprite(player,
               PspNum::Flash,
               static_cast<StateNum>(
                   toIndex(weaponinfo[toIndex(player.readyweapon)].flashstate)
                   + (randomness().forPlay() & 1)));

    spawnPlayerMissile(*player.mo, MobjType::Plasma);
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
    angle_t an = mo.angle;
    auto aim = aimLineAttack(&mo, an, 16 * 64 * FRACUNIT);
    scratch.bulletslope = aim.slope;

    if (!aim.target)
    {
        an += angle_t {1u << 26};
        aim = aimLineAttack(&mo, an, 16 * 64 * FRACUNIT);
        scratch.bulletslope = aim.slope;
        if (!aim.target)
        {
            an -= angle_t {2u << 26};
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
    angle_t angle = mo.angle;

    if (!accurate)
        angle += angle_t {
            (unsigned) (randomness().forPlay() - randomness().forPlay()) << 18};

    lineAttack(mo, angle, MISSILERANGE, weaponScratch().bulletslope, damage);
}

//
// firePistol
//
void firePistol(Player& player, PspDef&)
{
    startSound(player.mo, SfxEnum::Pistol);

    setMobjState(*player.mo, StateNum::PlayAtk2);
    player.ammo[toIndex(weaponinfo[toIndex(player.readyweapon)].ammo)]--;

    setPsprite(player,
               PspNum::Flash,
               static_cast<StateNum>(
                   toIndex(weaponinfo[toIndex(player.readyweapon)].flashstate)));

    computeBulletSlope(*player.mo);
    gunShot(*player.mo, !player.refire);
}

//
// fireShotgun
//
void fireShotgun(Player& player, PspDef&)
{
    startSound(player.mo, SfxEnum::Shotgn);
    setMobjState(*player.mo, StateNum::PlayAtk2);

    player.ammo[toIndex(weaponinfo[toIndex(player.readyweapon)].ammo)]--;

    setPsprite(player,
               PspNum::Flash,
               static_cast<StateNum>(
                   toIndex(weaponinfo[toIndex(player.readyweapon)].flashstate)));

    computeBulletSlope(*player.mo);

    for (int i = 0; i < 7; i++)
        gunShot(*player.mo, false);
}

//
// fireShotgun2
//
void fireShotgun2(Player& player, PspDef&)
{
    startSound(player.mo, SfxEnum::Dshtgn);
    setMobjState(*player.mo, StateNum::PlayAtk2);

    player.ammo[toIndex(weaponinfo[toIndex(player.readyweapon)].ammo)] -= 2;

    setPsprite(player,
               PspNum::Flash,
               static_cast<StateNum>(
                   toIndex(weaponinfo[toIndex(player.readyweapon)].flashstate)));

    computeBulletSlope(*player.mo);

    for (int i = 0; i < 20; i++)
    {
        int damage = 5 * (randomness().forPlay() % 3 + 1);
        angle_t angle = player.mo->angle;
        angle += angle_t {
            (unsigned) (randomness().forPlay() - randomness().forPlay()) << 19};
        lineAttack(
            *player.mo,
            angle,
            MISSILERANGE,
            weaponScratch().bulletslope
                + fixed_t {(randomness().forPlay() - randomness().forPlay()) << 5},
            damage);
    }
}

//
// fireCGun
//
void fireCGun(Player& player, PspDef& psp)
{
    startSound(player.mo, SfxEnum::Pistol);

    if (!player.ammo[toIndex(weaponinfo[toIndex(player.readyweapon)].ammo)])
        return;

    setMobjState(*player.mo, StateNum::PlayAtk2);
    player.ammo[toIndex(weaponinfo[toIndex(player.readyweapon)].ammo)]--;

    setPsprite(player,
               PspNum::Flash,
               static_cast<StateNum>(
                   toIndex(weaponinfo[toIndex(player.readyweapon)].flashstate)
                   + psp.state - &states[toIndex(StateNum::Chain1)]));

    computeBulletSlope(*player.mo);

    gunShot(*player.mo, !player.refire);
}

//
// ?
//
void light0(Player& player, PspDef&)
{
    player.extralight = 0;
}

void light1(Player& player, PspDef&)
{
    player.extralight = 1;
}

void light2(Player& player, PspDef&)
{
    player.extralight = 2;
}

//
// bfgSpray
// Spawn a BFG explosion on every monster in view
//
void bfgSpray(Mobj& mo)
{
    // offset angles from its attack angle
    for (int i = 0; i < 40; i++)
    {
        angle_t an = mo.angle - ang90 / 2 + ang90 / 40 * i;

        // mo.target is the originator (player)
        //  of the missile
        const auto aim = aimLineAttack(mo.target, an, 16 * 64 * FRACUNIT);

        if (!aim.target)
            continue;

        spawnMobj(aim.target->x,
                  aim.target->y,
                  aim.target->z + (aim.target->height >> 2),
                  MobjType::Extrabfg);

        int damage = 0;
        for (int j = 0; j < 15; j++)
            damage += (randomness().forPlay() & 7) + 1;

        damageMobj(*aim.target, mo.target, mo.target, damage);
    }
}

//
// bfgSound
//
void bfgSound(Player& player, PspDef&)
{
    startSound(player.mo, SfxEnum::Bfg);
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
