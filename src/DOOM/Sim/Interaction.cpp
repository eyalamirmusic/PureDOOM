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

#include "../Host/Platform.h"

// Data.
#include "../Game/GameDefs.h"
#include "../Game/Strings.h"
#include "../Game/SoundData.h"

#include "../Game/MapSpawns.h"

#include "../UI/AutomapTypes.h"
#include "Random.h"
#include "SimDefs.h"

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

namespace Doom
{

constexpr int BONUSADD = 6;

//
// GET STUFF
//

//
// giveAmmo
// Num is the number of clip loads,
// not the individual count (0= 1/2 clip).
// Returns false if the ammo can't be picked up at all
//

bool giveAmmo(Player& player, AmmoType ammo, int num)
{
    auto& ammoLimit = ammoLimits();

    if (ammo == AmmoType::NoAmmo)
        return false;

    // Vanilla's own bounds test, kept exactly: `> numAmmo`, not `>=`, so the
    // NumAmmo sentinel itself passes through. Preserved, not tightened.
    if (toIndex(ammo) < 0 || toIndex(ammo) > numAmmo)
    {
        //fatalError ("giveAmmo: bad type %i", ammo);

        fatalError("giveAmmo: bad type ", static_cast<int>(ammo));
    }

    if (player.ammo[toIndex(ammo)] == player.maxammo[toIndex(ammo)])
        return false;

    if (num)
        num *= ammoLimit.clipammo[toIndex(ammo)];
    else
        num = ammoLimit.clipammo[toIndex(ammo)] / 2;

    const auto skill = gameSession().gameskill;

    if (skill == Skill::Baby || skill == Skill::Nightmare)
    {
        // give double ammo in trainer mode,
        // you'll need in nightmare
        num <<= 1;
    }

    int oldammo = player.ammo[toIndex(ammo)];
    player.ammo[toIndex(ammo)] += num;

    if (player.ammo[toIndex(ammo)] > player.maxammo[toIndex(ammo)])
        player.ammo[toIndex(ammo)] = player.maxammo[toIndex(ammo)];

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
        case AmmoType::Clip:
            if (player.readyweapon == WeaponType::Fist)
            {
                if (player.weaponowned[toIndex(WeaponType::Chaingun)])
                    player.pendingweapon = WeaponType::Chaingun;
                else
                    player.pendingweapon = WeaponType::Pistol;
            }
            break;

        case AmmoType::Shell:
            if (player.readyweapon == WeaponType::Fist
                || player.readyweapon == WeaponType::Pistol)
            {
                if (player.weaponowned[toIndex(WeaponType::Shotgun)])
                    player.pendingweapon = WeaponType::Shotgun;
            }
            break;

        case AmmoType::Cell:
            if (player.readyweapon == WeaponType::Fist
                || player.readyweapon == WeaponType::Pistol)
            {
                if (player.weaponowned[toIndex(WeaponType::Plasma)])
                    player.pendingweapon = WeaponType::Plasma;
            }
            break;

        case AmmoType::Misl:
            if (player.readyweapon == WeaponType::Fist)
            {
                if (player.weaponowned[toIndex(WeaponType::Missile)])
                    player.pendingweapon = WeaponType::Missile;
            }
            break;

        // NoAmmo returned at the top of the function and NumAmmo is the count
        // sentinel; both are listed so the switch covers the enum.
        case AmmoType::NumAmmo:
        case AmmoType::NoAmmo:
            break;
    }

    return true;
}

//
// giveWeapon
// The weapon name may have a MF_DROPPED flag ored in.
//
bool giveWeapon(Player& player, WeaponType weapon, bool dropped)
{
    bool gaveammo;
    bool gaveweapon;

    const auto& session = gameSession();

    if (session.netgame && (session.deathmatch != 2) && !dropped)
    {
        // leave placed weapons forever on net games
        if (player.weaponowned[toIndex(weapon)])
            return false;

        player.bonuscount += BONUSADD;
        player.weaponowned[toIndex(weapon)] = true;

        if (session.deathmatch)
            giveAmmo(player, weaponinfo[toIndex(weapon)].ammo, 5);
        else
            giveAmmo(player, weaponinfo[toIndex(weapon)].ammo, 2);
        player.pendingweapon = weapon;

        const auto& players_ = playerState();

        if (&player == &players_.players[players_.consoleplayer])
            startSound(nullptr, SfxEnum::Wpnup);
        return false;
    }

    if (weaponinfo[toIndex(weapon)].ammo != AmmoType::NoAmmo)
    {
        // give one clip with a dropped weapon,
        // two clips with a found weapon
        if (dropped)
            gaveammo = giveAmmo(player, weaponinfo[toIndex(weapon)].ammo, 1);
        else
            gaveammo = giveAmmo(player, weaponinfo[toIndex(weapon)].ammo, 2);
    }
    else
        gaveammo = false;

    if (player.weaponowned[toIndex(weapon)])
        gaveweapon = false;
    else
    {
        gaveweapon = true;
        player.weaponowned[toIndex(weapon)] = true;
        player.pendingweapon = weapon;
    }

    return (gaveweapon || gaveammo);
}

//
// giveBody
// Returns false if the body isn't needed at all
//
bool giveBody(Player& player, int num)
{
    if (player.health >= MAXHEALTH)
        return false;

    player.health += num;
    if (player.health > MAXHEALTH)
        player.health = MAXHEALTH;
    player.mo->health = player.health;

    return true;
}

//
// giveArmor
// Returns false if the armor is worse
// than the current armor.
//
bool giveArmor(Player& player, int armortype)
{
    int hits = armortype * 100;
    if (player.armorpoints >= hits)
        return false; // don't pick up

    player.armortype = armortype;
    player.armorpoints = hits;

    return true;
}

//
// giveCard
//
void giveCard(Player& player, Card card)
{
    if (player.cards[toIndex(card)])
        return;

    player.bonuscount = BONUSADD;
    player.cards[toIndex(card)] = true;
}

//
// givePower
//
bool givePower(Player& player, PowerType power)
{
    if (power == PowerType::Invulnerability)
    {
        player.powers[toIndex(power)] = invulnTics;
        return true;
    }

    if (power == PowerType::Invisibility)
    {
        player.powers[toIndex(power)] = invisTics;
        player.mo->flags |= MF_SHADOW;
        return true;
    }

    if (power == PowerType::Infrared)
    {
        player.powers[toIndex(power)] = infraTics;
        return true;
    }

    if (power == PowerType::IronFeet)
    {
        player.powers[toIndex(power)] = ironTics;
        return true;
    }

    if (power == PowerType::Strength)
    {
        giveBody(player, 100);
        player.powers[toIndex(power)] = 1;
        return true;
    }

    if (player.powers[toIndex(power)])
        return false; // already got it

    player.powers[toIndex(power)] = 1;
    return true;
}

//
// touchSpecialThing
//
void touchSpecialThing(Mobj& special, Mobj& toucher)
{
    fixed_t delta = special.z - toucher.z;

    if (delta > toucher.height || delta < -8 * FRACUNIT)
    {
        // out of reach
        return;
    }

    SfxEnum sound = SfxEnum::Itemup;
    Player* player = toucher.player;

    const auto& session = gameSession();

    // Dead thing touching.
    // Can happen with a sliding player corpse.
    if (toucher.health <= 0)
        return;

    // Identify by sprite.
    switch (special.sprite)
    {
            // armor
        case SpriteNum::Arm1:
            if (!giveArmor(*player, 1))
                return;
            player->message = GOTARMOR;
            break;

        case SpriteNum::Arm2:
            if (!giveArmor(*player, 2))
                return;
            player->message = GOTMEGA;
            break;

            // bonus items
        case SpriteNum::Bon1:
            player->health++; // can go over 100%
            if (player->health > 200)
                player->health = 200;
            player->mo->health = player->health;
            player->message = GOTHTHBONUS;
            break;

        case SpriteNum::Bon2:
            player->armorpoints++; // can go over 100%
            if (player->armorpoints > 200)
                player->armorpoints = 200;
            if (!player->armortype)
                player->armortype = 1;
            player->message = GOTARMBONUS;
            break;

        case SpriteNum::Soul:
            player->health += 100;
            if (player->health > 200)
                player->health = 200;
            player->mo->health = player->health;
            player->message = GOTSUPER;
            sound = SfxEnum::Getpow;
            break;

        case SpriteNum::Mega:
            if (gameVersion().gamemode != GameMode::Commercial)
                return;
            player->health = 200;
            player->mo->health = player->health;
            giveArmor(*player, 2);
            player->message = GOTMSPHERE;
            sound = SfxEnum::Getpow;
            break;

            // cards
            // leave cards for everyone
        case SpriteNum::Bkey:
            if (!player->cards[toIndex(Card::BlueCard)])
                player->message = GOTBLUECARD;
            giveCard(*player, Card::BlueCard);
            if (!session.netgame)
                break;
            return;

        case SpriteNum::Ykey:
            if (!player->cards[toIndex(Card::YellowCard)])
                player->message = GOTYELWCARD;
            giveCard(*player, Card::YellowCard);
            if (!session.netgame)
                break;
            return;

        case SpriteNum::Rkey:
            if (!player->cards[toIndex(Card::RedCard)])
                player->message = GOTREDCARD;
            giveCard(*player, Card::RedCard);
            if (!session.netgame)
                break;
            return;

        case SpriteNum::Bsku:
            if (!player->cards[toIndex(Card::BlueSkull)])
                player->message = GOTBLUESKUL;
            giveCard(*player, Card::BlueSkull);
            if (!session.netgame)
                break;
            return;

        case SpriteNum::Ysku:
            if (!player->cards[toIndex(Card::YellowSkull)])
                player->message = GOTYELWSKUL;
            giveCard(*player, Card::YellowSkull);
            if (!session.netgame)
                break;
            return;

        case SpriteNum::Rsku:
            if (!player->cards[toIndex(Card::RedSkull)])
                player->message = GOTREDSKULL;
            giveCard(*player, Card::RedSkull);
            if (!session.netgame)
                break;
            return;

            // medikits, heals
        case SpriteNum::Stim:
            if (!giveBody(*player, 10))
                return;
            player->message = GOTSTIM;
            break;

        case SpriteNum::Medi:
            if (!giveBody(*player, 25))
                return;

            if (player->health < 25)
                player->message = GOTMEDINEED;
            else
                player->message = GOTMEDIKIT;
            break;

            // power ups
        case SpriteNum::Pinv:
            if (!givePower(*player, PowerType::Invulnerability))
                return;
            player->message = GOTINVUL;
            sound = SfxEnum::Getpow;
            break;

        case SpriteNum::Pstr:
            if (!givePower(*player, PowerType::Strength))
                return;
            player->message = GOTBERSERK;
            if (player->readyweapon != WeaponType::Fist)
                player->pendingweapon = WeaponType::Fist;
            sound = SfxEnum::Getpow;
            break;

        case SpriteNum::Pins:
            if (!givePower(*player, PowerType::Invisibility))
                return;
            player->message = GOTINVIS;
            sound = SfxEnum::Getpow;
            break;

        case SpriteNum::Suit:
            if (!givePower(*player, PowerType::IronFeet))
                return;
            player->message = GOTSUIT;
            sound = SfxEnum::Getpow;
            break;

        case SpriteNum::Pmap:
            if (!givePower(*player, PowerType::AllMap))
                return;
            player->message = GOTMAP;
            sound = SfxEnum::Getpow;
            break;

        case SpriteNum::Pvis:
            if (!givePower(*player, PowerType::Infrared))
                return;
            player->message = GOTVISOR;
            sound = SfxEnum::Getpow;
            break;

            // ammo
        case SpriteNum::Clip:
            if (special.flags & MF_DROPPED)
            {
                if (!giveAmmo(*player, AmmoType::Clip, 0))
                    return;
            }
            else
            {
                if (!giveAmmo(*player, AmmoType::Clip, 1))
                    return;
            }
            player->message = GOTCLIP;
            break;

        case SpriteNum::Ammo:
            if (!giveAmmo(*player, AmmoType::Clip, 5))
                return;
            player->message = GOTCLIPBOX;
            break;

        case SpriteNum::Rock:
            if (!giveAmmo(*player, AmmoType::Misl, 1))
                return;
            player->message = GOTROCKET;
            break;

        case SpriteNum::Brok:
            if (!giveAmmo(*player, AmmoType::Misl, 5))
                return;
            player->message = GOTROCKBOX;
            break;

        case SpriteNum::Cell:
            if (!giveAmmo(*player, AmmoType::Cell, 1))
                return;
            player->message = GOTCELL;
            break;

        case SpriteNum::Celp:
            if (!giveAmmo(*player, AmmoType::Cell, 5))
                return;
            player->message = GOTCELLBOX;
            break;

        case SpriteNum::Shel:
            if (!giveAmmo(*player, AmmoType::Shell, 1))
                return;
            player->message = GOTSHELLS;
            break;

        case SpriteNum::Sbox:
            if (!giveAmmo(*player, AmmoType::Shell, 5))
                return;
            player->message = GOTSHELLBOX;
            break;

        case SpriteNum::Bpak:
            if (!player->backpack)
            {
                for (int i = 0; i < numAmmo; i++)
                    player->maxammo[i] *= 2;
                player->backpack = true;
            }
            for (int i = 0; i < numAmmo; i++)
                giveAmmo(*player, static_cast<AmmoType>(i), 1);
            player->message = GOTBACKPACK;
            break;

            // weapons
        case SpriteNum::Bfug:
            if (!giveWeapon(*player, WeaponType::Bfg, false))
                return;
            player->message = GOTBFG9000;
            sound = SfxEnum::Wpnup;
            break;

        case SpriteNum::Mgun:
            if (!giveWeapon(
                    *player, WeaponType::Chaingun, special.flags & MF_DROPPED))
                return;
            player->message = GOTCHAINGUN;
            sound = SfxEnum::Wpnup;
            break;

        case SpriteNum::Csaw:
            if (!giveWeapon(*player, WeaponType::Chainsaw, false))
                return;
            player->message = GOTCHAINSAW;
            sound = SfxEnum::Wpnup;
            break;

        case SpriteNum::Laun:
            if (!giveWeapon(*player, WeaponType::Missile, false))
                return;
            player->message = GOTLAUNCHER;
            sound = SfxEnum::Wpnup;
            break;

        case SpriteNum::Plas:
            if (!giveWeapon(*player, WeaponType::Plasma, false))
                return;
            player->message = GOTPLASMA;
            sound = SfxEnum::Wpnup;
            break;

        case SpriteNum::Shot:
            if (!giveWeapon(
                    *player, WeaponType::Shotgun, special.flags & MF_DROPPED))
                return;
            player->message = GOTSHOTGUN;
            sound = SfxEnum::Wpnup;
            break;

        case SpriteNum::Sgn2:
            if (!giveWeapon(
                    *player, WeaponType::SuperShotgun, special.flags & MF_DROPPED))
                return;
            player->message = GOTSHOTGUN2;
            sound = SfxEnum::Wpnup;
            break;

        default:
            fatalError("P_SpecialThing: Unknown gettable thing");
    }

    if (special.flags & MF_COUNTITEM)
        player->itemcount++;
    removeMobj(special);
    player->bonuscount += BONUSADD;

    const auto& players_ = playerState();

    if (player == &players_.players[players_.consoleplayer])
        startSound(nullptr, sound);
}

//
// KillMobj
//
void killMobj(Mobj* source, Mobj& target)
{
    MobjType item;

    target.flags &= ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY);

    if (target.type != MobjType::Skull)
        target.flags &= ~MF_NOGRAVITY;

    target.flags |= MF_CORPSE | MF_DROPOFF;
    target.height >>= 2;

    auto& players_ = playerState();

    if (source && source->player)
    {
        // count for intermission
        if (target.flags & MF_COUNTKILL)
            source->player->killcount++;

        if (target.player)
            source->player->frags[target.player - players_.players.data()]++;
    }
    else if (!gameSession().netgame && (target.flags & MF_COUNTKILL))
    {
        // count all monster deaths,
        // even those caused by other monsters
        players_.players[0].killcount++;
    }

    if (target.player)
    {
        // count environment kills against you
        if (!source)
            target.player->frags[target.player - players_.players.data()]++;

        target.flags &= ~MF_SOLID;
        target.player->playerstate = PlayerLifeState::Dead;
        dropWeapon(*target.player);

        if (target.player == &players_.players[players_.consoleplayer]
            && overlayState().automapactive)
        {
            // don't die in auto map,
            // switch view prior to dying
            stopAutomap();
        }
    }

    if (target.health < -target.info->spawnhealth
        && target.info->xdeathstate != StateNum::Null)
    {
        setMobjState(target, target.info->xdeathstate);
    }
    else
        setMobjState(target, static_cast<StateNum>(target.info->deathstate));
    target.tics -= randomness().forPlay() & 3;

    if (target.tics < 1)
        target.tics = 1;

    //        startSoundHost (&actor->r, actor->info->deathsound);

    // Drop stuff.
    // This determines the kind of object spawned
    // during the death frame of a thing.
    switch (target.type)
    {
        case MobjType::Wolfss:
        case MobjType::Possessed:
            item = MobjType::Clip;
            break;

        case MobjType::Shotguy:
            item = MobjType::Shotgun;
            break;

        case MobjType::Chainguy:
            item = MobjType::Chaingun;
            break;

        default:
            return;
    }

    Mobj* mo = spawnMobj(target.x, target.y, ONFLOORZ, item);
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
void damageMobj(Mobj& target, Mobj* inflictor, Mobj* source, int damage)
{
    angle_t ang {};
    int saved;

    if (!(target.flags & MF_SHOOTABLE))
        return; // shouldn't happen...

    if (target.health <= 0)
        return;

    if (target.flags & MF_SKULLFLY)
    {
        target.momx = target.momy = target.momz = fixed_t {};
    }

    Player* player = target.player;
    if (player && gameSession().gameskill == Skill::Baby)
        damage >>= 1; // take half damage in trainer mode

    // Some close combat weapons should not
    // inflict thrust and push the victim out of reach,
    // thus kick away unless using the chainsaw.
    if (inflictor && !(target.flags & MF_NOCLIP)
        && (!source || !source->player
            || source->player->readyweapon != WeaponType::Chainsaw))
    {
        ang = pointToAngle2(inflictor->x, inflictor->y, target.x, target.y);

        fixed_t thrust = damage * (FRACUNIT >> 3) * 100 / target.info->mass;

        // make fall forwards sometimes
        if (damage < 40 && damage > target.health
            && target.z - inflictor->z > 64 * FRACUNIT
            && (randomness().forPlay() & 1))
        {
            ang += ang180;
            thrust = thrust * 4;
        }

        const auto angFine = ang.fineIndex();
        target.momx += FixedMul(thrust, finecosine[angFine]);
        target.momy += FixedMul(thrust, finesine[angFine]);
    }

    // player specific
    if (player)
    {
        // end of game hell hack
        if (target.subsector->sector->special == 11 && damage >= target.health)
        {
            damage = target.health - 1;
        }

        // Below certain threshold,
        // ignore damage in GOD mode, or with INVUL power.
        if (damage < 1000
            && ((player->cheats & CF_GODMODE)
                || player->powers[toIndex(PowerType::Invulnerability)]))
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

        int temp = damage < 100 ? damage : 100;

        const auto& players_ = playerState();

        if (player == &players_.players[players_.consoleplayer])
            tactileFeedback(40, 10, 40 + temp * 2);
    }

    // do the damage
    target.health -= damage;
    if (target.health <= 0)
    {
        killMobj(source, target);
        return;
    }

    if ((randomness().forPlay() < target.info->painchance)
        && !(target.flags & MF_SKULLFLY))
    {
        target.flags |= MF_JUSTHIT; // fight back!

        setMobjState(target, static_cast<StateNum>(target.info->painstate));
    }

    target.reactiontime = 0; // we're awake now...

    if ((!target.threshold || target.type == MobjType::Vile) && source
        && source != &target && source->type != MobjType::Vile)
    {
        // if not intent on another player,
        // chase after this one
        target.target = source;
        target.threshold = BASETHRESHOLD;
        if (target.state == &states[toIndex(target.info->spawnstate)]
            && target.info->seestate != StateNum::Null)
            setMobjState(target, static_cast<StateNum>(target.info->seestate));
    }
}
} // namespace Doom
