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
#include "Clip.h"
#include "Weapon.h"
#include "WeaponScratch.h"

#include "../Game/Sound.h"
#include "../Render/Main.h"
#include "Enemy.h"
#include "Interaction.h"
#include "MapAction.h"
#include "Mobj.h"
#include "Random.h"
#define LOWERSPEED (FRACUNIT * 6)
#define RAISESPEED (FRACUNIT * 6)

#define WEAPONBOTTOM (128 * FRACUNIT)
#define WEAPONTOP (32 * FRACUNIT)

// plasma cells for a bfg attack
#define BFGCELLS 40

namespace Doom
{
// The auto-aim slope of the shot being fired; file-local. The weapon scratch now lives on the
// Engine (Sim/WeaponScratch.h, moved by the file-scope-statics sweep - REFACTOR.md, Step 5).
// computeBulletSlope hoists weaponScratch() once and reaches bulletslope through it; gunShot and
// fireShotgun2 each touch it exactly once and reach it inline, rather than through file-scope
// reference aliases (REFACTOR.md, Step 9 strand (a)).

// Forward declarations so call order needs no rearranging.
void bringUpWeapon(Player* player);
doom_boolean checkAmmo(Player* player);
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
void gunShot(Mobj* mo, doom_boolean accurate);
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
    PspDef* psp;
    State* state;

    psp = &player->psprites[position];

    do
    {
        if (!stnum)
        {
            // object removed itself
            psp->state = nullptr;
            break;
        }

        state = &states[stnum];
        psp->state = state;
        psp->tics = state->tics; // could be 0

        if (state->misc1)
        {
            // coordinate set
            psp->sx = Doom::Fixed::fromInt(state->misc1);
            psp->sy = Doom::Fixed::fromInt(state->misc2);
        }

        // Call action routine. The action is stored type-erased; a weapon state
        // carries a (Player*, PspDef*) action, cast back from the erased pointer
        // here (a round-trip conversion, hence well-defined).
        if (state->action.fn)
        {
            reinterpret_cast<void (*)(Player*, PspDef*)>(state->action.fn)(
                player, psp);
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
    StateNum newstate;

    if (player->pendingweapon == wp_nochange)
        player->pendingweapon = player->readyweapon;

    if (player->pendingweapon == wp_chainsaw)
        Doom::startSound(player->mo, sfx_sawup);

    newstate = static_cast<StateNum>(weaponinfo[player->pendingweapon].upstate);

    player->pendingweapon = wp_nochange;
    player->psprites[ps_weapon].sy = WEAPONBOTTOM;

    setPsprite(player, ps_weapon, newstate);
}

//
// checkAmmo
// Returns true if there is enough ammo to shoot.
// If not, selects the next weapon to use.
//
doom_boolean checkAmmo(Player* player)
{
    AmmoType ammo;
    int count;

    ammo = weaponinfo[player->readyweapon].ammo;

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
    StateNum newstate;

    if (!checkAmmo(player))
        return;

    Doom::setMobjState(player->mo, S_PLAY_ATK1);
    newstate = static_cast<StateNum>(weaponinfo[player->readyweapon].atkstate);
    setPsprite(player, ps_weapon, newstate);
    Doom::noiseAlert(player->mo, *player->mo);

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
    StateNum newstate;
    int angle;

    // get out of attack state
    if (player.mo->state == &states[S_PLAY_ATK1]
        || player.mo->state == &states[S_PLAY_ATK2])
    {
        Doom::setMobjState(player.mo, S_PLAY);
    }

    if (player.readyweapon == wp_chainsaw && psp.state == &states[S_SAW])
    {
        Doom::startSound(player.mo, sfx_sawidl);
    }

    // check for change
    //  if player is dead, put the weapon away
    if (player.pendingweapon != wp_nochange || !player.health)
    {
        // change weapon
        //  (pending weapon should allready be validated)
        newstate =
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
    angle = (128 * levelStats().leveltime) & FINEMASK;
    psp.sx = FRACUNIT + FixedMul(player.bob, finecosine[angle]);
    angle &= FINEANGLES / 2 - 1;
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
    StateNum newstate;

    psp.sy -= RAISESPEED;

    if (psp.sy > WEAPONTOP)
        return;

    psp.sy = WEAPONTOP;

    // The weapon has been raised all the way,
    //  so change to the ready state.
    newstate = static_cast<StateNum>(weaponinfo[player.readyweapon].readystate);

    setPsprite(&player, ps_weapon, newstate);
}

//
// gunFlash
//
void gunFlash(Player& player, PspDef&)
{
    Doom::setMobjState(player.mo, S_PLAY_ATK2);
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
    angle_t angle;
    int damage;
    fixed_t slope;

    auto& c = clip();

    damage = (Doom::randomness().forPlay() % 10 + 1) << 1;

    if (player.powers[pw_strength])
        damage *= 10;

    angle = player.mo->angle;
    angle += angle_t {(unsigned) (Doom::randomness().forPlay()
                              - Doom::randomness().forPlay())
                  << 18};
    slope = Doom::aimLineAttack(player.mo, angle, MELEERANGE);
    Doom::lineAttack(player.mo, angle, MELEERANGE, slope, damage);

    // turn to face target
    if (c.linetarget)
    {
        Doom::startSound(player.mo, sfx_punch);
        player.mo->angle = Doom::pointToAngle2(
            player.mo->x, player.mo->y, c.linetarget->x, c.linetarget->y);
    }
}

//
// saw
//
void saw(Player& player, PspDef&)
{
    angle_t angle;
    int damage;
    fixed_t slope;

    auto& c = clip();

    damage = 2 * (Doom::randomness().forPlay() % 10 + 1);
    angle = player.mo->angle;
    angle += angle_t {(unsigned) (Doom::randomness().forPlay()
                              - Doom::randomness().forPlay())
                  << 18};

    // use meleerange + 1 se the puff doesn't skip the flash
    slope = Doom::aimLineAttack(player.mo, angle, MELEERANGE + fixed_t {1});
    Doom::lineAttack(player.mo, angle, MELEERANGE + fixed_t {1}, slope, damage);

    if (!c.linetarget)
    {
        Doom::startSound(player.mo, sfx_sawful);
        return;
    }
    Doom::startSound(player.mo, sfx_sawhit);

    // turn to face target
    angle = Doom::pointToAngle2(
        player.mo->x, player.mo->y, c.linetarget->x, c.linetarget->y);
    if (angle - player.mo->angle > ANG180)
    {
        if (angle - player.mo->angle < static_cast<angle_t>(-ANG90 / 20))
            player.mo->angle = angle + ANG90 / 21;
        else
            player.mo->angle -= ANG90 / 20;
    }
    else
    {
        if (angle - player.mo->angle > ANG90 / 20)
            player.mo->angle = angle - ANG90 / 21;
        else
            player.mo->angle += ANG90 / 20;
    }
    player.mo->flags |= MF_JUSTATTACKED;
}

//
// fireMissile
//
void fireMissile(Player& player, PspDef&)
{
    player.ammo[weaponinfo[player.readyweapon].ammo]--;
    Doom::spawnPlayerMissile(player.mo, MT_ROCKET);
}

//
// fireBFG
//
void fireBFG(Player& player, PspDef&)
{
    player.ammo[weaponinfo[player.readyweapon].ammo] -= BFGCELLS;
    Doom::spawnPlayerMissile(player.mo, MT_BFG);
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
                                       + (Doom::randomness().forPlay() & 1)));

    Doom::spawnPlayerMissile(player.mo, MT_PLASMA);
}

//
// computeBulletSlope
// Sets a slope so a near miss is at aproximately
// the height of the intended target
//
void computeBulletSlope(Mobj* mo)
{
    angle_t an;

    auto& c = clip();
    auto& scratch = weaponScratch();

    // see which target is to be aimed at
    an = mo->angle;
    scratch.bulletslope = Doom::aimLineAttack(mo, an, 16 * 64 * FRACUNIT);

    if (!c.linetarget)
    {
        an += angle_t {1u << 26};
        scratch.bulletslope = Doom::aimLineAttack(mo, an, 16 * 64 * FRACUNIT);
        if (!c.linetarget)
        {
            an -= angle_t {2u << 26};
            scratch.bulletslope = Doom::aimLineAttack(mo, an, 16 * 64 * FRACUNIT);
        }
    }
}

//
// gunShot
//
void gunShot(Mobj* mo, doom_boolean accurate)
{
    angle_t angle;
    int damage;

    damage = 5 * (Doom::randomness().forPlay() % 3 + 1);
    angle = mo->angle;

    if (!accurate)
        angle += angle_t {(unsigned) (Doom::randomness().forPlay()
                              - Doom::randomness().forPlay())
                  << 18};

    Doom::lineAttack(mo, angle, MISSILERANGE, weaponScratch().bulletslope, damage);
}

//
// firePistol
//
void firePistol(Player& player, PspDef&)
{
    Doom::startSound(player.mo, sfx_pistol);

    Doom::setMobjState(player.mo, S_PLAY_ATK2);
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
    Doom::startSound(player.mo, sfx_shotgn);
    Doom::setMobjState(player.mo, S_PLAY_ATK2);

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
    angle_t angle;
    int damage;

    Doom::startSound(player.mo, sfx_dshtgn);
    Doom::setMobjState(player.mo, S_PLAY_ATK2);

    player.ammo[weaponinfo[player.readyweapon].ammo] -= 2;

    setPsprite(&player,
               ps_flash,
               static_cast<StateNum>(weaponinfo[player.readyweapon].flashstate));

    computeBulletSlope(player.mo);

    for (int i = 0; i < 20; i++)
    {
        damage = 5 * (Doom::randomness().forPlay() % 3 + 1);
        angle = player.mo->angle;
        angle += angle_t {(unsigned) (Doom::randomness().forPlay()
                              - Doom::randomness().forPlay())
                  << 19};
        Doom::lineAttack(player.mo,
                     angle,
                     MISSILERANGE,
                     weaponScratch().bulletslope
                         + fixed_t {(Doom::randomness().forPlay()
                                     - Doom::randomness().forPlay())
                                    << 5},
                     damage);
    }
}

//
// fireCGun
//
void fireCGun(Player& player, PspDef& psp)
{
    Doom::startSound(player.mo, sfx_pistol);

    if (!player.ammo[weaponinfo[player.readyweapon].ammo])
        return;

    Doom::setMobjState(player.mo, S_PLAY_ATK2);
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
    int damage;
    angle_t an;

    auto& c = clip();

    // offset angles from its attack angle
    for (int i = 0; i < 40; i++)
    {
        an = mo->angle - ANG90 / 2 + ANG90 / 40 * i;

        // mo->target is the originator (player)
        //  of the missile
        Doom::aimLineAttack(mo->target, an, 16 * 64 * FRACUNIT);

        if (!c.linetarget)
            continue;

        Doom::spawnMobj(c.linetarget->x,
                    c.linetarget->y,
                    c.linetarget->z + (c.linetarget->height >> 2),
                    MT_EXTRABFG);

        damage = 0;
        for (int j = 0; j < 15; j++)
            damage += (Doom::randomness().forPlay() & 7) + 1;

        Doom::damageMobj(c.linetarget, mo->target, mo->target, damage);
    }
}

//
// bfgSound
//
void bfgSound(Player& player, PspDef&)
{
    Doom::startSound(player.mo, sfx_bfg);
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
    PspDef* psp;
    State* state;

    psp = &player.psprites[0];
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
