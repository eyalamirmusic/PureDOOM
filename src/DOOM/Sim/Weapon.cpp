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
// stay global). swingx/swingy/bulletslope are file-local here now.
//
//-----------------------------------------------------------------------------

#include "../doom_config.h"

#include "../d_event.h"
#include "../doomdef.h"
#include "../doomstat.h"
#include "../m_random.h"
#include "../p_local.h"
#include "../s_sound.h"
#include "../sounds.h"

#include "Weapon.h"
#include "WeaponScratch.h"

#define LOWERSPEED (FRACUNIT * 6)
#define RAISESPEED (FRACUNIT * 6)

#define WEAPONBOTTOM (128 * FRACUNIT)
#define WEAPONTOP (32 * FRACUNIT)

// plasma cells for a bfg attack
#define BFGCELLS 40

namespace Doom
{
// Weapon-bob offset and the auto-aim slope of the shot being fired; file-local.
// The weapon scratch now lives on the Engine (Sim/WeaponScratch.h, moved by the file-scope-statics
// sweep - REFACTOR.md, Step 5). The vanilla names are references onto that member; read by no other
// file.
static fixed_t& swingx = weaponScratch().swingx;
static fixed_t& swingy = weaponScratch().swingy;
static fixed_t& bulletslope = weaponScratch().bulletslope;

// Forward declarations so call order needs no rearranging.
void bringUpWeapon(player_t* player);
doom_boolean checkAmmo(player_t* player);
void fireWeapon(player_t* player);
void dropWeapon(player_t* player);
void weaponReady(player_t* player, pspdef_t* psp);
void reFire(player_t* player, pspdef_t* psp);
void checkReload(player_t* player, pspdef_t* psp);
void lower(player_t* player, pspdef_t* psp);
void raise(player_t* player, pspdef_t* psp);
void gunFlash(player_t* player, pspdef_t* psp);
void punch(player_t* player, pspdef_t* psp);
void saw(player_t* player, pspdef_t* psp);
void fireMissile(player_t* player, pspdef_t* psp);
void fireBFG(player_t* player, pspdef_t* psp);
void firePlasma(player_t* player, pspdef_t* psp);
void computeBulletSlope(mobj_t* mo);
void gunShot(mobj_t* mo, doom_boolean accurate);
void firePistol(player_t* player, pspdef_t* psp);
void fireShotgun(player_t* player, pspdef_t* psp);
void fireShotgun2(player_t* player, pspdef_t* psp);
void fireCGun(player_t* player, pspdef_t* psp);
void light0(player_t* player, pspdef_t* psp);
void light1(player_t* player, pspdef_t* psp);
void light2(player_t* player, pspdef_t* psp);
void bfgSpray(mobj_t* mo);
void bfgSound(player_t* player, pspdef_t* psp);
void setupPsprites(player_t* player);
void movePsprites(player_t* player);

void setPsprite(player_t* player, int position, statenum_t stnum)
{
    pspdef_t* psp;
    state_t* state;

    psp = &player->psprites[position];

    do
    {
        if (!stnum)
        {
            // object removed itself
            psp->state = 0;
            break;
        }

        state = &states[stnum];
        psp->state = state;
        psp->tics = state->tics; // could be 0

        if (state->misc1)
        {
            // coordinate set
            psp->sx = state->misc1 << FRACBITS;
            psp->sy = state->misc2 << FRACBITS;
        }

        // Call action routine.
        // Modified handling.
        if (state->action.acp2)
        {
            state->action.acp2(player, psp);
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
void bringUpWeapon(player_t* player)
{
    statenum_t newstate;

    if (player->pendingweapon == wp_nochange)
        player->pendingweapon = player->readyweapon;

    if (player->pendingweapon == wp_chainsaw)
        S_StartSound(player->mo, sfx_sawup);

    newstate = (statenum_t) (weaponinfo[player->pendingweapon].upstate);

    player->pendingweapon = wp_nochange;
    player->psprites[ps_weapon].sy = WEAPONBOTTOM;

    setPsprite(player, ps_weapon, newstate);
}

//
// checkAmmo
// Returns true if there is enough ammo to shoot.
// If not, selects the next weapon to use.
//
doom_boolean checkAmmo(player_t* player)
{
    ammotype_t ammo;
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
    do
    {
        if (player->weaponowned[wp_plasma] && player->ammo[am_cell]
            && (gamemode != shareware))
        {
            player->pendingweapon = wp_plasma;
        }
        else if (player->weaponowned[wp_supershotgun] && player->ammo[am_shell] > 2
                 && (gamemode == commercial))
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
                 && (gamemode != shareware))
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
    setPsprite(
        player, ps_weapon, (statenum_t) (weaponinfo[player->readyweapon].downstate));

    return false;
}

//
// fireWeapon.
//
void fireWeapon(player_t* player)
{
    statenum_t newstate;

    if (!checkAmmo(player))
        return;

    P_SetMobjState(player->mo, S_PLAY_ATK1);
    newstate = (statenum_t) (weaponinfo[player->readyweapon].atkstate);
    setPsprite(player, ps_weapon, newstate);
    P_NoiseAlert(player->mo, player->mo);

    // [pd] Stop gun bobbing when shooting
    pspdef_t* psp;
    psp = &player->psprites[ps_weapon];
    psp->sx = FRACUNIT;
    psp->sy = WEAPONTOP;
}

//
// dropWeapon
// Player died, so put the weapon away.
//
void dropWeapon(player_t* player)
{
    setPsprite(
        player, ps_weapon, (statenum_t) (weaponinfo[player->readyweapon].downstate));
}

//
// weaponReady
// The player can fire the weapon
// or change to another weapon at this time.
// Follows after getting weapon up,
// or after previous attack/fire sequence.
//
void weaponReady(player_t* player, pspdef_t* psp)
{
    statenum_t newstate;
    int angle;

    // get out of attack state
    if (player->mo->state == &states[S_PLAY_ATK1]
        || player->mo->state == &states[S_PLAY_ATK2])
    {
        P_SetMobjState(player->mo, S_PLAY);
    }

    if (player->readyweapon == wp_chainsaw && psp->state == &states[S_SAW])
    {
        S_StartSound(player->mo, sfx_sawidl);
    }

    // check for change
    //  if player is dead, put the weapon away
    if (player->pendingweapon != wp_nochange || !player->health)
    {
        // change weapon
        //  (pending weapon should allready be validated)
        newstate = (statenum_t) (weaponinfo[player->readyweapon].downstate);
        setPsprite(player, ps_weapon, newstate);
        return;
    }

    // check for fire
    //  the missile launcher and bfg do not auto fire
    if (player->cmd.buttons & BT_ATTACK)
    {
        if (!player->attackdown
            || (player->readyweapon != wp_missile && player->readyweapon != wp_bfg))
        {
            player->attackdown = true;
            fireWeapon(player);
            return;
        }
    }
    else
        player->attackdown = false;

    // bob the weapon based on movement speed
    angle = (128 * leveltime) & FINEMASK;
    psp->sx = FRACUNIT + FixedMul(player->bob, finecosine[angle]);
    angle &= FINEANGLES / 2 - 1;
    psp->sy = WEAPONTOP + FixedMul(player->bob, finesine[angle]);
}

//
// reFire
// The player can re-fire the weapon
// without lowering it entirely.
//
void reFire(player_t* player, pspdef_t*)
{
    // check for fire
    //  (if a weaponchange is pending, let it go through instead)
    if ((player->cmd.buttons & BT_ATTACK) && player->pendingweapon == wp_nochange
        && player->health)
    {
        player->refire++;
        fireWeapon(player);
    }
    else
    {
        player->refire = 0;
        checkAmmo(player);
    }
}

void checkReload(player_t* player, pspdef_t*)
{
    checkAmmo(player);
}

//
// lower
// Lowers current weapon,
//  and changes weapon at bottom.
//
void lower(player_t* player, pspdef_t* psp)
{
    psp->sy += LOWERSPEED;

    // Is already down.
    if (psp->sy < WEAPONBOTTOM)
        return;

    // Player is dead.
    if (player->playerstate == PST_DEAD)
    {
        psp->sy = WEAPONBOTTOM;

        // don't bring weapon back up
        return;
    }

    // The old weapon has been lowered off the screen,
    // so change the weapon and start raising it
    if (!player->health)
    {
        // Player is dead, so keep the weapon off screen.
        setPsprite(player, ps_weapon, S_NULL);
        return;
    }

    player->readyweapon = player->pendingweapon;

    bringUpWeapon(player);
}

//
// raise
//
void raise(player_t* player, pspdef_t* psp)
{
    statenum_t newstate;

    psp->sy -= RAISESPEED;

    if (psp->sy > WEAPONTOP)
        return;

    psp->sy = WEAPONTOP;

    // The weapon has been raised all the way,
    //  so change to the ready state.
    newstate = (statenum_t) (weaponinfo[player->readyweapon].readystate);

    setPsprite(player, ps_weapon, newstate);
}

//
// gunFlash
//
void gunFlash(player_t* player, pspdef_t*)
{
    P_SetMobjState(player->mo, S_PLAY_ATK2);
    setPsprite(
        player, ps_flash, (statenum_t) (weaponinfo[player->readyweapon].flashstate));
}

//
// WEAPON ATTACKS
//

//
// punch
//
void punch(player_t* player, pspdef_t*)
{
    angle_t angle;
    int damage;
    int slope;

    damage = (P_Random() % 10 + 1) << 1;

    if (player->powers[pw_strength])
        damage *= 10;

    angle = player->mo->angle;
    angle += (P_Random() - P_Random()) << 18;
    slope = P_AimLineAttack(player->mo, angle, MELEERANGE);
    P_LineAttack(player->mo, angle, MELEERANGE, slope, damage);

    // turn to face target
    if (linetarget)
    {
        S_StartSound(player->mo, sfx_punch);
        player->mo->angle = R_PointToAngle2(
            player->mo->x, player->mo->y, linetarget->x, linetarget->y);
    }
}

//
// saw
//
void saw(player_t* player, pspdef_t*)
{
    angle_t angle;
    int damage;
    int slope;

    damage = 2 * (P_Random() % 10 + 1);
    angle = player->mo->angle;
    angle += (P_Random() - P_Random()) << 18;

    // use meleerange + 1 se the puff doesn't skip the flash
    slope = P_AimLineAttack(player->mo, angle, MELEERANGE + 1);
    P_LineAttack(player->mo, angle, MELEERANGE + 1, slope, damage);

    if (!linetarget)
    {
        S_StartSound(player->mo, sfx_sawful);
        return;
    }
    S_StartSound(player->mo, sfx_sawhit);

    // turn to face target
    angle =
        R_PointToAngle2(player->mo->x, player->mo->y, linetarget->x, linetarget->y);
    if (angle - player->mo->angle > ANG180)
    {
        if (angle - player->mo->angle < (angle_t) (-ANG90 / 20))
            player->mo->angle = angle + ANG90 / 21;
        else
            player->mo->angle -= ANG90 / 20;
    }
    else
    {
        if (angle - player->mo->angle > ANG90 / 20)
            player->mo->angle = angle - ANG90 / 21;
        else
            player->mo->angle += ANG90 / 20;
    }
    player->mo->flags |= MF_JUSTATTACKED;
}

//
// fireMissile
//
void fireMissile(player_t* player, pspdef_t*)
{
    player->ammo[weaponinfo[player->readyweapon].ammo]--;
    P_SpawnPlayerMissile(player->mo, MT_ROCKET);
}

//
// fireBFG
//
void fireBFG(player_t* player, pspdef_t*)
{
    player->ammo[weaponinfo[player->readyweapon].ammo] -= BFGCELLS;
    P_SpawnPlayerMissile(player->mo, MT_BFG);
}

//
// firePlasma
//
void firePlasma(player_t* player, pspdef_t*)
{
    player->ammo[weaponinfo[player->readyweapon].ammo]--;

    setPsprite(player,
               ps_flash,
               (statenum_t) (weaponinfo[player->readyweapon].flashstate
                             + (P_Random() & 1)));

    P_SpawnPlayerMissile(player->mo, MT_PLASMA);
}

//
// computeBulletSlope
// Sets a slope so a near miss is at aproximately
// the height of the intended target
//
void computeBulletSlope(mobj_t* mo)
{
    angle_t an;

    // see which target is to be aimed at
    an = mo->angle;
    bulletslope = P_AimLineAttack(mo, an, 16 * 64 * FRACUNIT);

    if (!linetarget)
    {
        an += 1 << 26;
        bulletslope = P_AimLineAttack(mo, an, 16 * 64 * FRACUNIT);
        if (!linetarget)
        {
            an -= 2 << 26;
            bulletslope = P_AimLineAttack(mo, an, 16 * 64 * FRACUNIT);
        }
    }
}

//
// gunShot
//
void gunShot(mobj_t* mo, doom_boolean accurate)
{
    angle_t angle;
    int damage;

    damage = 5 * (P_Random() % 3 + 1);
    angle = mo->angle;

    if (!accurate)
        angle += (P_Random() - P_Random()) << 18;

    P_LineAttack(mo, angle, MISSILERANGE, bulletslope, damage);
}

//
// firePistol
//
void firePistol(player_t* player, pspdef_t*)
{
    S_StartSound(player->mo, sfx_pistol);

    P_SetMobjState(player->mo, S_PLAY_ATK2);
    player->ammo[weaponinfo[player->readyweapon].ammo]--;

    setPsprite(
        player, ps_flash, (statenum_t) (weaponinfo[player->readyweapon].flashstate));

    computeBulletSlope(player->mo);
    gunShot(player->mo, !player->refire);
}

//
// fireShotgun
//
void fireShotgun(player_t* player, pspdef_t*)
{
    int i;

    S_StartSound(player->mo, sfx_shotgn);
    P_SetMobjState(player->mo, S_PLAY_ATK2);

    player->ammo[weaponinfo[player->readyweapon].ammo]--;

    setPsprite(
        player, ps_flash, (statenum_t) (weaponinfo[player->readyweapon].flashstate));

    computeBulletSlope(player->mo);

    for (i = 0; i < 7; i++)
        gunShot(player->mo, false);
}

//
// fireShotgun2
//
void fireShotgun2(player_t* player, pspdef_t*)
{
    int i;
    angle_t angle;
    int damage;

    S_StartSound(player->mo, sfx_dshtgn);
    P_SetMobjState(player->mo, S_PLAY_ATK2);

    player->ammo[weaponinfo[player->readyweapon].ammo] -= 2;

    setPsprite(
        player, ps_flash, (statenum_t) (weaponinfo[player->readyweapon].flashstate));

    computeBulletSlope(player->mo);

    for (i = 0; i < 20; i++)
    {
        damage = 5 * (P_Random() % 3 + 1);
        angle = player->mo->angle;
        angle += (P_Random() - P_Random()) << 19;
        P_LineAttack(player->mo,
                     angle,
                     MISSILERANGE,
                     bulletslope + ((P_Random() - P_Random()) << 5),
                     damage);
    }
}

//
// fireCGun
//
void fireCGun(player_t* player, pspdef_t* psp)
{
    S_StartSound(player->mo, sfx_pistol);

    if (!player->ammo[weaponinfo[player->readyweapon].ammo])
        return;

    P_SetMobjState(player->mo, S_PLAY_ATK2);
    player->ammo[weaponinfo[player->readyweapon].ammo]--;

    setPsprite(player,
               ps_flash,
               (statenum_t) (weaponinfo[player->readyweapon].flashstate + psp->state
                             - &states[S_CHAIN1]));

    computeBulletSlope(player->mo);

    gunShot(player->mo, !player->refire);
}

//
// ?
//
void light0(player_t* player, pspdef_t*)
{
    player->extralight = 0;
}

void light1(player_t* player, pspdef_t*)
{
    player->extralight = 1;
}

void light2(player_t* player, pspdef_t*)
{
    player->extralight = 2;
}

//
// bfgSpray
// Spawn a BFG explosion on every monster in view
//
void bfgSpray(mobj_t* mo)
{
    int i;
    int j;
    int damage;
    angle_t an;

    // offset angles from its attack angle
    for (i = 0; i < 40; i++)
    {
        an = mo->angle - ANG90 / 2 + ANG90 / 40 * i;

        // mo->target is the originator (player)
        //  of the missile
        P_AimLineAttack(mo->target, an, 16 * 64 * FRACUNIT);

        if (!linetarget)
            continue;

        P_SpawnMobj(linetarget->x,
                    linetarget->y,
                    linetarget->z + (linetarget->height >> 2),
                    MT_EXTRABFG);

        damage = 0;
        for (j = 0; j < 15; j++)
            damage += (P_Random() & 7) + 1;

        P_DamageMobj(linetarget, mo->target, mo->target, damage);
    }
}

//
// bfgSound
//
void bfgSound(player_t* player, pspdef_t*)
{
    S_StartSound(player->mo, sfx_bfg);
}

//
// setupPsprites
// Called at start of level for each player.
//
void setupPsprites(player_t* player)
{
    int i;

    // remove all psprites
    for (i = 0; i < NUMPSPRITES; i++)
        player->psprites[i].state = 0;

    // spawn the gun
    player->pendingweapon = player->readyweapon;
    bringUpWeapon(player);
}

//
// movePsprites
// Called every tic by player thinking routine.
//
void movePsprites(player_t* player)
{
    int i;
    pspdef_t* psp;
    state_t* state;

    psp = &player->psprites[0];
    for (i = 0; i < NUMPSPRITES; i++, psp++)
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
                    setPsprite(player, i, psp->state->nextstate);
            }
        }
    }

    player->psprites[ps_flash].sx = player->psprites[ps_weapon].sx;
    player->psprites[ps_flash].sy = player->psprites[ps_weapon].sy;
}
} // namespace Doom
