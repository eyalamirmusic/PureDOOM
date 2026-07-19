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
// The suppression is scoped to this one function and spelled for each compiler,
// which is the point of the wrapper: the two disagree on the flag's name, and
// naming the wrong one is itself a warning - GCC raises -Wpragmas for Clang's
// spelling, which is how three stale suppressions in this tree were found. Clang
// must be tested before GCC, since it defines __GNUC__ as well.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type-mismatch"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

void callWeaponAction(actionf_p1 action, Player* player, PspDef* psp)
{
    reinterpret_cast<void (*)(Player*, PspDef*)>(action)(player, psp);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
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
void bringUpWeapon(Player* player);
bool checkAmmo(Player* player);
void fireWeapon(Player* player);
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
void computeBulletSlope(Mobj* mo);
void gunShot(Mobj* mo, bool accurate);
void firePistol(Player& player, PspDef& psp);
void fireShotgun(Player& player, PspDef& psp);
void fireShotgun2(Player& player, PspDef& psp);
void fireCGun(Player& player, PspDef& psp);
void light0(Player& player, PspDef& psp);
void light1(Player& player, PspDef& psp);
void light2(Player& player, PspDef& psp);
void bfgSpray(Mobj* mo);
void bfgSound(Player& player, PspDef& psp);
void setupPsprites(Player& player);
void movePsprites(Player& player);

void setPsprite(Player* player, int position, StateNum stnum)
{
    PspDef* psp = &player->psprites[position];

    do
    {
        if (!stnum)
        {
            // object removed itself
            psp->state = nullptr;
            break;
        }

        State* state = &states[stnum];
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
            callWeaponAction(state->action.fn, player, psp);
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
void bringUpWeapon(Player* player)
{
    if (player->pendingweapon == wp_nochange)
        player->pendingweapon = player->readyweapon;

    if (player->pendingweapon == wp_chainsaw)
        startSound(player->mo, sfx_sawup);

    StateNum newstate =
        static_cast<StateNum>(weaponinfo[player->pendingweapon].upstate);

    player->pendingweapon = wp_nochange;
    player->psprites[ps_weapon].sy = WEAPONBOTTOM;

    setPsprite(player, ps_weapon, newstate);
}

//
// checkAmmo
// Returns true if there is enough ammo to shoot.
// If not, selects the next weapon to use.
//
bool checkAmmo(Player* player)
{
    int count;

    AmmoType ammo = weaponinfo[player->readyweapon].ammo;

    // Minimal amount for one shot varies.
    if (player->readyweapon == wp_bfg)
        count = BFGCELLS;
    else if (player->readyweapon == wp_supershotgun)
        count = 2; // Double barrel.
    else
        count = 1; // Regular.

    // Some do not need ammunition anyway.
    // Return if current ammunition sufficient.
    if (ammo == am_noammo || player->ammo[ammo] >= count)
        return true;

    // Out of ammo, pick a weapon to change to.
    // Preferences are set here.
    const auto& version = gameVersion();

    do
    {
        if (player->weaponowned[wp_plasma] && player->ammo[am_cell]
            && (version.gamemode != shareware))
        {
            player->pendingweapon = wp_plasma;
        }
        else if (player->weaponowned[wp_supershotgun] && player->ammo[am_shell] > 2
                 && (version.gamemode == commercial))
        {
            player->pendingweapon = wp_supershotgun;
        }
        else if (player->weaponowned[wp_chaingun] && player->ammo[am_clip])
        {
            player->pendingweapon = wp_chaingun;
        }
        else if (player->weaponowned[wp_shotgun] && player->ammo[am_shell])
        {
            player->pendingweapon = wp_shotgun;
        }
        else if (player->ammo[am_clip])
        {
            player->pendingweapon = wp_pistol;
        }
        else if (player->weaponowned[wp_chainsaw])
        {
            player->pendingweapon = wp_chainsaw;
        }
        else if (player->weaponowned[wp_missile] && player->ammo[am_misl])
        {
            player->pendingweapon = wp_missile;
        }
        else if (player->weaponowned[wp_bfg] && player->ammo[am_cell] > 40
                 && (version.gamemode != shareware))
        {
            player->pendingweapon = wp_bfg;
        }
        else
        {
            // If everything fails.
            player->pendingweapon = wp_fist;
        }

    } while (player->pendingweapon == wp_nochange);

    // Now set appropriate weapon overlay.
    setPsprite(player,
               ps_weapon,
               static_cast<StateNum>(weaponinfo[player->readyweapon].downstate));

    return false;
}

//
// fireWeapon.
//
void fireWeapon(Player* player)
{
    if (!checkAmmo(player))
        return;

    setMobjState(player->mo, S_PLAY_ATK1);
    StateNum newstate =
        static_cast<StateNum>(weaponinfo[player->readyweapon].atkstate);
    setPsprite(player, ps_weapon, newstate);
    noiseAlert(player->mo, *player->mo);

    // [pd] Stop gun bobbing when shooting
    PspDef* psp = &player->psprites[ps_weapon];
    psp->sx = FRACUNIT;
    psp->sy = WEAPONTOP;
}

//
// dropWeapon
// Player died, so put the weapon away.
//
void dropWeapon(Player& player)
{
    setPsprite(&player,
               ps_weapon,
               static_cast<StateNum>(weaponinfo[player.readyweapon].downstate));
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
    if (player.mo->state == &states[S_PLAY_ATK1]
        || player.mo->state == &states[S_PLAY_ATK2])
    {
        setMobjState(player.mo, S_PLAY);
    }

    if (player.readyweapon == wp_chainsaw && psp.state == &states[S_SAW])
    {
        startSound(player.mo, sfx_sawidl);
    }

    // check for change
    //  if player is dead, put the weapon away
    if (player.pendingweapon != wp_nochange || !player.health)
    {
        // change weapon
        //  (pending weapon should allready be validated)
        StateNum newstate =
            static_cast<StateNum>(weaponinfo[player.readyweapon].downstate);
        setPsprite(&player, ps_weapon, newstate);
        return;
    }

    // check for fire
    //  the missile launcher and bfg do not auto fire
    if (player.cmd.buttons & BT_ATTACK)
    {
        if (!player.attackdown
            || (player.readyweapon != wp_missile && player.readyweapon != wp_bfg))
        {
            player.attackdown = true;
            fireWeapon(&player);
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
    if ((player.cmd.buttons & BT_ATTACK) && player.pendingweapon == wp_nochange
        && player.health)
    {
        player.refire++;
        fireWeapon(&player);
    }
    else
    {
        player.refire = 0;
        checkAmmo(&player);
    }
}

void checkReload(Player& player, PspDef&)
{
    checkAmmo(&player);
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
    if (player.playerstate == PST_DEAD)
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
        setPsprite(&player, ps_weapon, S_NULL);
        return;
    }

    player.readyweapon = player.pendingweapon;

    bringUpWeapon(&player);
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
        static_cast<StateNum>(weaponinfo[player.readyweapon].readystate);

    setPsprite(&player, ps_weapon, newstate);
}

//
// gunFlash
//
void gunFlash(Player& player, PspDef&)
{
    setMobjState(player.mo, S_PLAY_ATK2);
    setPsprite(&player,
               ps_flash,
               static_cast<StateNum>(weaponinfo[player.readyweapon].flashstate));
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

    if (player.powers[pw_strength])
        damage *= 10;

    angle_t angle = player.mo->angle;
    angle += angle_t {
        (unsigned) (randomness().forPlay() - randomness().forPlay())
        << 18};
    const auto aim = aimLineAttack(player.mo, angle, MELEERANGE);
    lineAttack(player.mo, angle, MELEERANGE, aim.slope, damage);

    // turn to face target
    if (aim.target)
    {
        startSound(player.mo, sfx_punch);
        player.mo->angle = pointToAngle2(
            player.mo->x, player.mo->y, aim.target->x, aim.target->y);
    }
}

//
// saw
//
void saw(Player& player, PspDef&)
{
    int damage = 2 * (randomness().forPlay() % 10 + 1);
    angle_t angle = player.mo->angle;
    angle += angle_t {
        (unsigned) (randomness().forPlay() - randomness().forPlay())
        << 18};

    // use meleerange + 1 se the puff doesn't skip the flash
    const auto aim = aimLineAttack(player.mo, angle, MELEERANGE + fixed_t {1});
    lineAttack(player.mo, angle, MELEERANGE + fixed_t {1}, aim.slope, damage);

    if (!aim.target)
    {
        startSound(player.mo, sfx_sawful);
        return;
    }
    startSound(player.mo, sfx_sawhit);

    // turn to face target
    angle = pointToAngle2(
        player.mo->x, player.mo->y, aim.target->x, aim.target->y);
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
    player.ammo[weaponinfo[player.readyweapon].ammo]--;
    spawnPlayerMissile(player.mo, MT_ROCKET);
}

//
// fireBFG
//
void fireBFG(Player& player, PspDef&)
{
    player.ammo[weaponinfo[player.readyweapon].ammo] -= BFGCELLS;
    spawnPlayerMissile(player.mo, MT_BFG);
}

//
// firePlasma
//
void firePlasma(Player& player, PspDef&)
{
    player.ammo[weaponinfo[player.readyweapon].ammo]--;

    setPsprite(&player,
               ps_flash,
               static_cast<StateNum>(weaponinfo[player.readyweapon].flashstate
                                     + (randomness().forPlay() & 1)));

    spawnPlayerMissile(player.mo, MT_PLASMA);
}

//
// computeBulletSlope
// Sets a slope so a near miss is at aproximately
// the height of the intended target
//
void computeBulletSlope(Mobj* mo)
{
    auto& scratch = weaponScratch();

    // see which target is to be aimed at
    angle_t an = mo->angle;
    auto aim = aimLineAttack(mo, an, 16 * 64 * FRACUNIT);
    scratch.bulletslope = aim.slope;

    if (!aim.target)
    {
        an += angle_t {1u << 26};
        aim = aimLineAttack(mo, an, 16 * 64 * FRACUNIT);
        scratch.bulletslope = aim.slope;
        if (!aim.target)
        {
            an -= angle_t {2u << 26};
            aim = aimLineAttack(mo, an, 16 * 64 * FRACUNIT);
            scratch.bulletslope = aim.slope;
        }
    }
}

//
// gunShot
//
void gunShot(Mobj* mo, bool accurate)
{
    int damage = 5 * (randomness().forPlay() % 3 + 1);
    angle_t angle = mo->angle;

    if (!accurate)
        angle += angle_t {
            (unsigned) (randomness().forPlay() - randomness().forPlay())
            << 18};

    lineAttack(mo, angle, MISSILERANGE, weaponScratch().bulletslope, damage);
}

//
// firePistol
//
void firePistol(Player& player, PspDef&)
{
    startSound(player.mo, sfx_pistol);

    setMobjState(player.mo, S_PLAY_ATK2);
    player.ammo[weaponinfo[player.readyweapon].ammo]--;

    setPsprite(&player,
               ps_flash,
               static_cast<StateNum>(weaponinfo[player.readyweapon].flashstate));

    computeBulletSlope(player.mo);
    gunShot(player.mo, !player.refire);
}

//
// fireShotgun
//
void fireShotgun(Player& player, PspDef&)
{
    startSound(player.mo, sfx_shotgn);
    setMobjState(player.mo, S_PLAY_ATK2);

    player.ammo[weaponinfo[player.readyweapon].ammo]--;

    setPsprite(&player,
               ps_flash,
               static_cast<StateNum>(weaponinfo[player.readyweapon].flashstate));

    computeBulletSlope(player.mo);

    for (int i = 0; i < 7; i++)
        gunShot(player.mo, false);
}

//
// fireShotgun2
//
void fireShotgun2(Player& player, PspDef&)
{
    startSound(player.mo, sfx_dshtgn);
    setMobjState(player.mo, S_PLAY_ATK2);

    player.ammo[weaponinfo[player.readyweapon].ammo] -= 2;

    setPsprite(&player,
               ps_flash,
               static_cast<StateNum>(weaponinfo[player.readyweapon].flashstate));

    computeBulletSlope(player.mo);

    for (int i = 0; i < 20; i++)
    {
        int damage = 5 * (randomness().forPlay() % 3 + 1);
        angle_t angle = player.mo->angle;
        angle += angle_t {
            (unsigned) (randomness().forPlay() - randomness().forPlay())
            << 19};
        lineAttack(player.mo,
                         angle,
                         MISSILERANGE,
                         weaponScratch().bulletslope
                             + fixed_t {(randomness().forPlay()
                                         - randomness().forPlay())
                                        << 5},
                         damage);
    }
}

//
// fireCGun
//
void fireCGun(Player& player, PspDef& psp)
{
    startSound(player.mo, sfx_pistol);

    if (!player.ammo[weaponinfo[player.readyweapon].ammo])
        return;

    setMobjState(player.mo, S_PLAY_ATK2);
    player.ammo[weaponinfo[player.readyweapon].ammo]--;

    setPsprite(&player,
               ps_flash,
               static_cast<StateNum>(weaponinfo[player.readyweapon].flashstate
                                     + psp.state - &states[S_CHAIN1]));

    computeBulletSlope(player.mo);

    gunShot(player.mo, !player.refire);
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
void bfgSpray(Mobj* mo)
{
    // offset angles from its attack angle
    for (int i = 0; i < 40; i++)
    {
        angle_t an = mo->angle - ang90 / 2 + ang90 / 40 * i;

        // mo->target is the originator (player)
        //  of the missile
        const auto aim = aimLineAttack(mo->target, an, 16 * 64 * FRACUNIT);

        if (!aim.target)
            continue;

        spawnMobj(aim.target->x,
                        aim.target->y,
                        aim.target->z + (aim.target->height >> 2),
                        MT_EXTRABFG);

        int damage = 0;
        for (int j = 0; j < 15; j++)
            damage += (randomness().forPlay() & 7) + 1;

        damageMobj(aim.target, mo->target, mo->target, damage);
    }
}

//
// bfgSound
//
void bfgSound(Player& player, PspDef&)
{
    startSound(player.mo, sfx_bfg);
}

//
// setupPsprites
// Called at start of level for each player.
//
void setupPsprites(Player& player)
{
    // remove all psprites
    for (int i = 0; i < NUMPSPRITES; i++)
        player.psprites[i].state = nullptr;

    // spawn the gun
    player.pendingweapon = player.readyweapon;
    bringUpWeapon(&player);
}

//
// movePsprites
// Called every tic by player thinking routine.
//
void movePsprites(Player& player)
{
    State* state;

    PspDef* psp = &player.psprites[0];
    for (int i = 0; i < NUMPSPRITES; i++, psp++)
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
                    setPsprite(&player, i, psp->state->nextstate);
            }
        }
    }

    player.psprites[ps_flash].sx = player.psprites[ps_weapon].sx;
    player.psprites[ps_flash].sy = player.psprites[ps_weapon].sy;
}
} // namespace Doom
