// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as published by id
// Software. All rights reserved. See the DOOM Source Code License for details.
//
// DESCRIPTION:
//        Handling interactions (i.e., collisions): pickups, damage, death.
//
// Rewritten into namespace Doom out of vanilla p_inter; p_inter.cpp keeps the
// vanilla names (Doom::touchSpecialThing, Doom::damageMobj, Doom::givePower) as shims and the
// maxammo/clipammo data tables. The give-* helpers stay Doom:: functions here,
// called only within this file. No mutable module state, so nothing moved to Clip.
//
//-----------------------------------------------------------------------------

#include "../doom_config.h"

// Data.
#include "../doomdef.h"
#include "../dstrings.h"
#include "../sounds.h"

#include "../doomstat.h"

#include "../am_map.h"
#include "../i_system.h"
#include "../m_random.h"
#include "../p_local.h"
#include "../s_sound.h"

#include "../Game/AmmoLimits.h"
#include "../Game/GameSession.h"
#include "../Game/GameVersion.h"
#include "../Game/OverlayState.h"
#include "../Game/PlayerState.h"
#include "Interaction.h"

#include "../UI/Automap.h"
#include "../Game/Sound.h"
#include "../Host/System.h"
#include "../Render/Main.h"
#include "Mobj.h"
#include "Weapon.h"
#include "Random.h"
#define BONUSADD 6

namespace Doom
{

//
// GET STUFF
//

//
// giveAmmo
// Num is the number of clip loads,
// not the individual count (0= 1/2 clip).
// Returns false if the ammo can't be picked up at all
//

doom_boolean giveAmmo(Player* player, AmmoType ammo, int num)
{
    int oldammo;

    auto& ammoLimit = ammoLimits();

    if (ammo == am_noammo)
        return false;

    if (ammo < 0 || ammo > NUMAMMO)
    {
        //fatalError ("giveAmmo: bad type %i", ammo);

        doom_strcpy(error_buf, "giveAmmo: bad type ");
        doom_concat(error_buf, doom_itoa(ammo, 10));
        fatalError(error_buf);
    }

    if (player->ammo[ammo] == player->maxammo[ammo])
        return false;

    if (num)
        num *= ammoLimit.clipammo[ammo];
    else
        num = ammoLimit.clipammo[ammo] / 2;

    const auto skill = gameSession().gameskill;

    if (skill == sk_baby || skill == sk_nightmare)
    {
        // give double ammo in trainer mode,
        // you'll need in nightmare
        num <<= 1;
    }

    oldammo = player->ammo[ammo];
    player->ammo[ammo] += num;

    if (player->ammo[ammo] > player->maxammo[ammo])
        player->ammo[ammo] = player->maxammo[ammo];

    // If non zero ammo,
    // don't change up weapons,
    // player was lower on purpose.
    if (oldammo)
        return true;

    // We were down to zero,
    // so select a new weapon.
    // Preferences are not user selectable.
    switch (ammo)
    {
        case am_clip:
            if (player->readyweapon == wp_fist)
            {
                if (player->weaponowned[wp_chaingun])
                    player->pendingweapon = wp_chaingun;
                else
                    player->pendingweapon = wp_pistol;
            }
            break;

        case am_shell:
            if (player->readyweapon == wp_fist || player->readyweapon == wp_pistol)
            {
                if (player->weaponowned[wp_shotgun])
                    player->pendingweapon = wp_shotgun;
            }
            break;

        case am_cell:
            if (player->readyweapon == wp_fist || player->readyweapon == wp_pistol)
            {
                if (player->weaponowned[wp_plasma])
                    player->pendingweapon = wp_plasma;
            }
            break;

        case am_misl:
            if (player->readyweapon == wp_fist)
            {
                if (player->weaponowned[wp_missile])
                    player->pendingweapon = wp_missile;
            }
        default:
            break;
    }

    return true;
}

//
// giveWeapon
// The weapon name may have a MF_DROPPED flag ored in.
//
doom_boolean giveWeapon(Player* player, WeaponType weapon, doom_boolean dropped)
{
    doom_boolean gaveammo;
    doom_boolean gaveweapon;

    const auto& session = gameSession();

    if (session.netgame && (session.deathmatch != 2) && !dropped)
    {
        // leave placed weapons forever on net games
        if (player->weaponowned[weapon])
            return false;

        player->bonuscount += BONUSADD;
        player->weaponowned[weapon] = true;

        if (session.deathmatch)
            giveAmmo(player, weaponinfo[weapon].ammo, 5);
        else
            giveAmmo(player, weaponinfo[weapon].ammo, 2);
        player->pendingweapon = weapon;

        const auto& players_ = playerState();

        if (player == &players_.players[players_.consoleplayer])
            Doom::startSound(nullptr, sfx_wpnup);
        return false;
    }

    if (weaponinfo[weapon].ammo != am_noammo)
    {
        // give one clip with a dropped weapon,
        // two clips with a found weapon
        if (dropped)
            gaveammo = giveAmmo(player, weaponinfo[weapon].ammo, 1);
        else
            gaveammo = giveAmmo(player, weaponinfo[weapon].ammo, 2);
    }
    else
        gaveammo = false;

    if (player->weaponowned[weapon])
        gaveweapon = false;
    else
    {
        gaveweapon = true;
        player->weaponowned[weapon] = true;
        player->pendingweapon = weapon;
    }

    return (gaveweapon || gaveammo);
}

//
// giveBody
// Returns false if the body isn't needed at all
//
doom_boolean giveBody(Player* player, int num)
{
    if (player->health >= MAXHEALTH)
        return false;

    player->health += num;
    if (player->health > MAXHEALTH)
        player->health = MAXHEALTH;
    player->mo->health = player->health;

    return true;
}

//
// giveArmor
// Returns false if the armor is worse
// than the current armor.
//
doom_boolean giveArmor(Player* player, int armortype)
{
    int hits = armortype * 100;
    if (player->armorpoints >= hits)
        return false; // don't pick up

    player->armortype = armortype;
    player->armorpoints = hits;

    return true;
}

//
// giveCard
//
void giveCard(Player* player, Card card)
{
    if (player->cards[card])
        return;

    player->bonuscount = BONUSADD;
    player->cards[card] = 1;
}

//
// givePower
//
doom_boolean givePower(Player* player, int /*PowerType*/ power)
{
    if (power == pw_invulnerability)
    {
        player->powers[power] = INVULNTICS;
        return true;
    }

    if (power == pw_invisibility)
    {
        player->powers[power] = INVISTICS;
        player->mo->flags |= MF_SHADOW;
        return true;
    }

    if (power == pw_infrared)
    {
        player->powers[power] = INFRATICS;
        return true;
    }

    if (power == pw_ironfeet)
    {
        player->powers[power] = IRONTICS;
        return true;
    }

    if (power == pw_strength)
    {
        giveBody(player, 100);
        player->powers[power] = 1;
        return true;
    }

    if (player->powers[power])
        return false; // already got it

    player->powers[power] = 1;
    return true;
}

//
// touchSpecialThing
//
void touchSpecialThing(Mobj* special, Mobj* toucher)
{
    Player* player;
    fixed_t delta;
    int sound;

    delta = special->z - toucher->z;

    if (delta > toucher->height || delta < -8 * FRACUNIT)
    {
        // out of reach
        return;
    }

    sound = sfx_itemup;
    player = toucher->player;

    const auto& session = gameSession();

    // Dead thing touching.
    // Can happen with a sliding player corpse.
    if (toucher->health <= 0)
        return;

    // Identify by sprite.
    switch (special->sprite)
    {
            // armor
        case SPR_ARM1:
            if (!giveArmor(player, 1))
                return;
            player->message = GOTARMOR;
            break;

        case SPR_ARM2:
            if (!giveArmor(player, 2))
                return;
            player->message = GOTMEGA;
            break;

            // bonus items
        case SPR_BON1:
            player->health++; // can go over 100%
            if (player->health > 200)
                player->health = 200;
            player->mo->health = player->health;
            player->message = GOTHTHBONUS;
            break;

        case SPR_BON2:
            player->armorpoints++; // can go over 100%
            if (player->armorpoints > 200)
                player->armorpoints = 200;
            if (!player->armortype)
                player->armortype = 1;
            player->message = GOTARMBONUS;
            break;

        case SPR_SOUL:
            player->health += 100;
            if (player->health > 200)
                player->health = 200;
            player->mo->health = player->health;
            player->message = GOTSUPER;
            sound = sfx_getpow;
            break;

        case SPR_MEGA:
            if (gameVersion().gamemode != commercial)
                return;
            player->health = 200;
            player->mo->health = player->health;
            giveArmor(player, 2);
            player->message = GOTMSPHERE;
            sound = sfx_getpow;
            break;

            // cards
            // leave cards for everyone
        case SPR_BKEY:
            if (!player->cards[it_bluecard])
                player->message = GOTBLUECARD;
            giveCard(player, it_bluecard);
            if (!session.netgame)
                break;
            return;

        case SPR_YKEY:
            if (!player->cards[it_yellowcard])
                player->message = GOTYELWCARD;
            giveCard(player, it_yellowcard);
            if (!session.netgame)
                break;
            return;

        case SPR_RKEY:
            if (!player->cards[it_redcard])
                player->message = GOTREDCARD;
            giveCard(player, it_redcard);
            if (!session.netgame)
                break;
            return;

        case SPR_BSKU:
            if (!player->cards[it_blueskull])
                player->message = GOTBLUESKUL;
            giveCard(player, it_blueskull);
            if (!session.netgame)
                break;
            return;

        case SPR_YSKU:
            if (!player->cards[it_yellowskull])
                player->message = GOTYELWSKUL;
            giveCard(player, it_yellowskull);
            if (!session.netgame)
                break;
            return;

        case SPR_RSKU:
            if (!player->cards[it_redskull])
                player->message = GOTREDSKULL;
            giveCard(player, it_redskull);
            if (!session.netgame)
                break;
            return;

            // medikits, heals
        case SPR_STIM:
            if (!giveBody(player, 10))
                return;
            player->message = GOTSTIM;
            break;

        case SPR_MEDI:
            if (!giveBody(player, 25))
                return;

            if (player->health < 25)
                player->message = GOTMEDINEED;
            else
                player->message = GOTMEDIKIT;
            break;

            // power ups
        case SPR_PINV:
            if (!givePower(player, pw_invulnerability))
                return;
            player->message = GOTINVUL;
            sound = sfx_getpow;
            break;

        case SPR_PSTR:
            if (!givePower(player, pw_strength))
                return;
            player->message = GOTBERSERK;
            if (player->readyweapon != wp_fist)
                player->pendingweapon = wp_fist;
            sound = sfx_getpow;
            break;

        case SPR_PINS:
            if (!givePower(player, pw_invisibility))
                return;
            player->message = GOTINVIS;
            sound = sfx_getpow;
            break;

        case SPR_SUIT:
            if (!givePower(player, pw_ironfeet))
                return;
            player->message = GOTSUIT;
            sound = sfx_getpow;
            break;

        case SPR_PMAP:
            if (!givePower(player, pw_allmap))
                return;
            player->message = GOTMAP;
            sound = sfx_getpow;
            break;

        case SPR_PVIS:
            if (!givePower(player, pw_infrared))
                return;
            player->message = GOTVISOR;
            sound = sfx_getpow;
            break;

            // ammo
        case SPR_CLIP:
            if (special->flags & MF_DROPPED)
            {
                if (!giveAmmo(player, am_clip, 0))
                    return;
            }
            else
            {
                if (!giveAmmo(player, am_clip, 1))
                    return;
            }
            player->message = GOTCLIP;
            break;

        case SPR_AMMO:
            if (!giveAmmo(player, am_clip, 5))
                return;
            player->message = GOTCLIPBOX;
            break;

        case SPR_ROCK:
            if (!giveAmmo(player, am_misl, 1))
                return;
            player->message = GOTROCKET;
            break;

        case SPR_BROK:
            if (!giveAmmo(player, am_misl, 5))
                return;
            player->message = GOTROCKBOX;
            break;

        case SPR_CELL:
            if (!giveAmmo(player, am_cell, 1))
                return;
            player->message = GOTCELL;
            break;

        case SPR_CELP:
            if (!giveAmmo(player, am_cell, 5))
                return;
            player->message = GOTCELLBOX;
            break;

        case SPR_SHEL:
            if (!giveAmmo(player, am_shell, 1))
                return;
            player->message = GOTSHELLS;
            break;

        case SPR_SBOX:
            if (!giveAmmo(player, am_shell, 5))
                return;
            player->message = GOTSHELLBOX;
            break;

        case SPR_BPAK:
            if (!player->backpack)
            {
                for (int i = 0; i < NUMAMMO; i++)
                    player->maxammo[i] *= 2;
                player->backpack = true;
            }
            for (int i = 0; i < NUMAMMO; i++)
                giveAmmo(player, static_cast<AmmoType>(i), 1);
            player->message = GOTBACKPACK;
            break;

            // weapons
        case SPR_BFUG:
            if (!giveWeapon(player, wp_bfg, false))
                return;
            player->message = GOTBFG9000;
            sound = sfx_wpnup;
            break;

        case SPR_MGUN:
            if (!giveWeapon(player, wp_chaingun, special->flags & MF_DROPPED))
                return;
            player->message = GOTCHAINGUN;
            sound = sfx_wpnup;
            break;

        case SPR_CSAW:
            if (!giveWeapon(player, wp_chainsaw, false))
                return;
            player->message = GOTCHAINSAW;
            sound = sfx_wpnup;
            break;

        case SPR_LAUN:
            if (!giveWeapon(player, wp_missile, false))
                return;
            player->message = GOTLAUNCHER;
            sound = sfx_wpnup;
            break;

        case SPR_PLAS:
            if (!giveWeapon(player, wp_plasma, false))
                return;
            player->message = GOTPLASMA;
            sound = sfx_wpnup;
            break;

        case SPR_SHOT:
            if (!giveWeapon(player, wp_shotgun, special->flags & MF_DROPPED))
                return;
            player->message = GOTSHOTGUN;
            sound = sfx_wpnup;
            break;

        case SPR_SGN2:
            if (!giveWeapon(player, wp_supershotgun, special->flags & MF_DROPPED))
                return;
            player->message = GOTSHOTGUN2;
            sound = sfx_wpnup;
            break;

        default:
            fatalError("P_SpecialThing: Unknown gettable thing");
    }

    if (special->flags & MF_COUNTITEM)
        player->itemcount++;
    Doom::removeMobj(special);
    player->bonuscount += BONUSADD;

    const auto& players_ = playerState();

    if (player == &players_.players[players_.consoleplayer])
        Doom::startSound(nullptr, sound);
}

//
// KillMobj
//
void killMobj(Mobj* source, Mobj* target)
{
    MobjType item;
    Mobj* mo;

    target->flags &= ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY);

    if (target->type != MT_SKULL)
        target->flags &= ~MF_NOGRAVITY;

    target->flags |= MF_CORPSE | MF_DROPOFF;
    target->height >>= 2;

    auto& players_ = playerState();

    if (source && source->player)
    {
        // count for intermission
        if (target->flags & MF_COUNTKILL)
            source->player->killcount++;

        if (target->player)
            source->player->frags[target->player - players_.players]++;
    }
    else if (!gameSession().netgame && (target->flags & MF_COUNTKILL))
    {
        // count all monster deaths,
        // even those caused by other monsters
        players_.players[0].killcount++;
    }

    if (target->player)
    {
        // count environment kills against you
        if (!source)
            target->player->frags[target->player - players_.players]++;

        target->flags &= ~MF_SOLID;
        target->player->playerstate = PST_DEAD;
        dropWeapon(*target->player);

        if (target->player == &players_.players[players_.consoleplayer]
            && overlayState().automapactive)
        {
            // don't die in auto map,
            // switch view prior to dying
            Doom::stopAutomap();
        }
    }

    if (target->health < -target->info->spawnhealth && target->info->xdeathstate)
    {
        Doom::setMobjState(target, static_cast<StateNum>(target->info->xdeathstate));
    }
    else
        Doom::setMobjState(target, static_cast<StateNum>(target->info->deathstate));
    target->tics -= Doom::randomness().forPlay() & 3;

    if (target->tics < 1)
        target->tics = 1;

    //        startSoundHost (&actor->r, actor->info->deathsound);

    // Drop stuff.
    // This determines the kind of object spawned
    // during the death frame of a thing.
    switch (target->type)
    {
        case MT_WOLFSS:
        case MT_POSSESSED:
            item = MT_CLIP;
            break;

        case MT_SHOTGUY:
            item = MT_SHOTGUN;
            break;

        case MT_CHAINGUY:
            item = MT_CHAINGUN;
            break;

        default:
            return;
    }

    mo = Doom::spawnMobj(target->x, target->y, ONFLOORZ, item);
    mo->flags |= MF_DROPPED; // special versions of items
}

//
// damageMobj
// Damages both enemies and players
// "inflictor" is the thing that caused the damage
//  creature or missile, can be 0 (slime, etc)
// "source" is the thing to target after taking damage
//  creature or 0
// Source and inflictor are the same for melee attacks.
// Source can be 0 for slime, barrel explosions
// and other environmental stuff.
//
void damageMobj(Mobj* target, Mobj* inflictor, Mobj* source, int damage)
{
    unsigned ang;
    int saved;
    Player* player;
    fixed_t thrust;
    int temp;

    if (!(target->flags & MF_SHOOTABLE))
        return; // shouldn't happen...

    if (target->health <= 0)
        return;

    if (target->flags & MF_SKULLFLY)
    {
        target->momx = target->momy = target->momz = 0;
    }

    player = target->player;
    if (player && gameSession().gameskill == sk_baby)
        damage >>= 1; // take half damage in trainer mode

    // Some close combat weapons should not
    // inflict thrust and push the victim out of reach,
    // thus kick away unless using the chainsaw.
    if (inflictor && !(target->flags & MF_NOCLIP)
        && (!source || !source->player
            || source->player->readyweapon != wp_chainsaw))
    {
        ang = Doom::pointToAngle2(inflictor->x, inflictor->y, target->x, target->y);

        thrust = damage * (FRACUNIT >> 3) * 100 / target->info->mass;

        // make fall forwards sometimes
        if (damage < 40 && damage > target->health
            && target->z - inflictor->z > 64 * FRACUNIT && (Doom::randomness().forPlay() & 1))
        {
            ang += ANG180;
            thrust *= 4;
        }

        ang >>= ANGLETOFINESHIFT;
        target->momx += FixedMul(thrust, finecosine[ang]);
        target->momy += FixedMul(thrust, finesine[ang]);
    }

    // player specific
    if (player)
    {
        // end of game hell hack
        if (target->subsector->sector->special == 11 && damage >= target->health)
        {
            damage = target->health - 1;
        }

        // Below certain threshold,
        // ignore damage in GOD mode, or with INVUL power.
        if (damage < 1000
            && ((player->cheats & CF_GODMODE) || player->powers[pw_invulnerability]))
        {
            return;
        }

        if (player->armortype)
        {
            if (player->armortype == 1)
                saved = damage / 3;
            else
                saved = damage / 2;

            if (player->armorpoints <= saved)
            {
                // armor is used up
                saved = player->armorpoints;
                player->armortype = 0;
            }
            player->armorpoints -= saved;
            damage -= saved;
        }
        player->health -= damage; // mirror mobj health here for Dave
        if (player->health < 0)
            player->health = 0;

        player->attacker = source;
        player->damagecount += damage; // add damage after armor / invuln

        if (player->damagecount > 100)
            player->damagecount = 100; // teleport stomp does 10k points...

        temp = damage < 100 ? damage : 100;

        const auto& players_ = playerState();

        if (player == &players_.players[players_.consoleplayer])
            tactileFeedback(40, 10, 40 + temp * 2);
    }

    // do the damage
    target->health -= damage;
    if (target->health <= 0)
    {
        killMobj(source, target);
        return;
    }

    if ((Doom::randomness().forPlay() < target->info->painchance) && !(target->flags & MF_SKULLFLY))
    {
        target->flags |= MF_JUSTHIT; // fight back!

        Doom::setMobjState(target, static_cast<StateNum>(target->info->painstate));
    }

    target->reactiontime = 0; // we're awake now...

    if ((!target->threshold || target->type == MT_VILE) && source && source != target
        && source->type != MT_VILE)
    {
        // if not intent on another player,
        // chase after this one
        target->target = source;
        target->threshold = BASETHRESHOLD;
        if (target->state == &states[target->info->spawnstate]
            && target->info->seestate != S_NULL)
            Doom::setMobjState(target, static_cast<StateNum>(target->info->seestate));
    }
}
} // namespace Doom
