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
// Rewritten into namespace Doom out of vanilla p_inter. One thing damaging another is
// Mobj::damage, and the pickups (the give-* helpers and givePower) are Player methods;
// see Interaction.h. touchSpecialThing and killMobj stay free functions - each takes
// two mobjs with no single owner. The maxammo/clipammo data tables live in
// Game/AmmoLimits.h. No mutable module state, so nothing moved to Clip.
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

bool Player::giveAmmo(AmmoType ammoType, int num)
{
    auto& ammoLimit = ammoLimits();

    if (ammoType == AmmoType::NoAmmo)
        return false;

    // Vanilla's own bounds test, kept exactly: `> numAmmo`, not `>=`, so the
    // NumAmmo sentinel itself passes through. Preserved, not tightened.
    if (toIndex(ammoType) < 0 || toIndex(ammoType) > numAmmo)
    {
        //fatalError ("giveAmmo: bad type %i", ammo);

        fatalError("giveAmmo: bad type ", static_cast<int>(ammoType));
    }

    if (ammo[toIndex(ammoType)] == maxammo[toIndex(ammoType)])
        return false;

    if (num)
        num *= ammoLimit.clipammo[toIndex(ammoType)];
    else
        num = ammoLimit.clipammo[toIndex(ammoType)] / 2;

    const auto skill = gameSession().gameskill;

    if (skill == Skill::Baby || skill == Skill::Nightmare)
    {
        // give double ammo in trainer mode,
        // you'll need in nightmare
        num <<= 1;
    }

    int oldammo = ammo[toIndex(ammoType)];
    ammo[toIndex(ammoType)] += num;

    if (ammo[toIndex(ammoType)] > maxammo[toIndex(ammoType)])
        ammo[toIndex(ammoType)] = maxammo[toIndex(ammoType)];

    // If non zero ammo,
    // don't change up weapons,
    // it was lower on purpose.
    if (oldammo)
        return true;

    // We were down to zero,
    // so select a new weapon.
    // Preferences are not user selectable.
    switch (ammoType)
    {
        case AmmoType::Clip:
            if (readyweapon == WeaponType::Fist)
            {
                if (weaponowned[toIndex(WeaponType::Chaingun)])
                    pendingweapon = WeaponType::Chaingun;
                else
                    pendingweapon = WeaponType::Pistol;
            }
            break;

        case AmmoType::Shell:
            if (readyweapon == WeaponType::Fist || readyweapon == WeaponType::Pistol)
            {
                if (weaponowned[toIndex(WeaponType::Shotgun)])
                    pendingweapon = WeaponType::Shotgun;
            }
            break;

        case AmmoType::Cell:
            if (readyweapon == WeaponType::Fist || readyweapon == WeaponType::Pistol)
            {
                if (weaponowned[toIndex(WeaponType::Plasma)])
                    pendingweapon = WeaponType::Plasma;
            }
            break;

        case AmmoType::Misl:
            if (readyweapon == WeaponType::Fist)
            {
                if (weaponowned[toIndex(WeaponType::Missile)])
                    pendingweapon = WeaponType::Missile;
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
// The weapon name may have a flagBits(MobjFlag::Dropped) flag ored in.
//
bool Player::giveWeapon(WeaponType weapon, bool dropped)
{
    bool gaveammo;
    bool gaveweapon;

    const auto& session = gameSession();

    if (session.netgame && (session.deathmatch != 2) && !dropped)
    {
        // leave placed weapons forever on net games
        if (weaponowned[toIndex(weapon)])
            return false;

        bonuscount += BONUSADD;
        weaponowned[toIndex(weapon)] = true;

        if (session.deathmatch)
            giveAmmo(weaponinfo()[toIndex(weapon)].ammo, 5);
        else
            giveAmmo(weaponinfo()[toIndex(weapon)].ammo, 2);
        pendingweapon = weapon;

        const auto& players_ = playerState();

        if (this == &players_.players[players_.consoleplayer])
            startSound(nullptr, SfxEnum::Wpnup);
        return false;
    }

    if (weaponinfo()[toIndex(weapon)].ammo != AmmoType::NoAmmo)
    {
        // give one clip with a dropped weapon,
        // two clips with a found weapon
        if (dropped)
            gaveammo = giveAmmo(weaponinfo()[toIndex(weapon)].ammo, 1);
        else
            gaveammo = giveAmmo(weaponinfo()[toIndex(weapon)].ammo, 2);
    }
    else
        gaveammo = false;

    if (weaponowned[toIndex(weapon)])
        gaveweapon = false;
    else
    {
        gaveweapon = true;
        weaponowned[toIndex(weapon)] = true;
        pendingweapon = weapon;
    }

    return (gaveweapon || gaveammo);
}

//
// giveBody
// Returns false if the body isn't needed at all
//
bool Player::giveBody(int num)
{
    if (health >= MAXHEALTH)
        return false;

    health += num;
    if (health > MAXHEALTH)
        health = MAXHEALTH;
    mo->health = health;

    return true;
}

//
// giveArmor
// Returns false if the armor is worse
// than the current armor.
//
bool Player::giveArmor(int armortypeToUse)
{
    int hits = armortypeToUse * 100;
    if (armorpoints >= hits)
        return false; // don't pick up

    armortype = armortypeToUse;
    armorpoints = hits;

    return true;
}

//
// giveCard
//
void Player::giveCard(Card card)
{
    if (cards[toIndex(card)])
        return;

    bonuscount = BONUSADD;
    cards[toIndex(card)] = true;
}

//
// givePower
//
bool Player::givePower(PowerType power)
{
    if (power == PowerType::Invulnerability)
    {
        powers[toIndex(power)] = invulnTics;
        return true;
    }

    if (power == PowerType::Invisibility)
    {
        powers[toIndex(power)] = invisTics;
        mo->flags = withFlags(mo->flags, MobjFlag::Shadow);
        return true;
    }

    if (power == PowerType::Infrared)
    {
        powers[toIndex(power)] = infraTics;
        return true;
    }

    if (power == PowerType::IronFeet)
    {
        powers[toIndex(power)] = ironTics;
        return true;
    }

    if (power == PowerType::Strength)
    {
        giveBody(100);
        powers[toIndex(power)] = 1;
        return true;
    }

    if (powers[toIndex(power)])
        return false; // already got it

    powers[toIndex(power)] = 1;
    return true;
}

//
// touchSpecialThing
//
void touchSpecialThing(Mobj& special, Mobj& toucher)
{
    Fixed delta = special.z - toucher.z;

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
            if (!player->giveArmor(1))
                return;
            player->message = GOTARMOR;
            break;

        case SpriteNum::Arm2:
            if (!player->giveArmor(2))
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
            player->giveArmor(2);
            player->message = GOTMSPHERE;
            sound = SfxEnum::Getpow;
            break;

            // cards
            // leave cards for everyone
        case SpriteNum::Bkey:
            if (!player->cards[toIndex(Card::BlueCard)])
                player->message = GOTBLUECARD;
            player->giveCard(Card::BlueCard);
            if (!session.netgame)
                break;
            return;

        case SpriteNum::Ykey:
            if (!player->cards[toIndex(Card::YellowCard)])
                player->message = GOTYELWCARD;
            player->giveCard(Card::YellowCard);
            if (!session.netgame)
                break;
            return;

        case SpriteNum::Rkey:
            if (!player->cards[toIndex(Card::RedCard)])
                player->message = GOTREDCARD;
            player->giveCard(Card::RedCard);
            if (!session.netgame)
                break;
            return;

        case SpriteNum::Bsku:
            if (!player->cards[toIndex(Card::BlueSkull)])
                player->message = GOTBLUESKUL;
            player->giveCard(Card::BlueSkull);
            if (!session.netgame)
                break;
            return;

        case SpriteNum::Ysku:
            if (!player->cards[toIndex(Card::YellowSkull)])
                player->message = GOTYELWSKUL;
            player->giveCard(Card::YellowSkull);
            if (!session.netgame)
                break;
            return;

        case SpriteNum::Rsku:
            if (!player->cards[toIndex(Card::RedSkull)])
                player->message = GOTREDSKULL;
            player->giveCard(Card::RedSkull);
            if (!session.netgame)
                break;
            return;

            // medikits, heals
        case SpriteNum::Stim:
            if (!player->giveBody(10))
                return;
            player->message = GOTSTIM;
            break;

        case SpriteNum::Medi:
            if (!player->giveBody(25))
                return;

            if (player->health < 25)
                player->message = GOTMEDINEED;
            else
                player->message = GOTMEDIKIT;
            break;

            // power ups
        case SpriteNum::Pinv:
            if (!player->givePower(PowerType::Invulnerability))
                return;
            player->message = GOTINVUL;
            sound = SfxEnum::Getpow;
            break;

        case SpriteNum::Pstr:
            if (!player->givePower(PowerType::Strength))
                return;
            player->message = GOTBERSERK;
            if (player->readyweapon != WeaponType::Fist)
                player->pendingweapon = WeaponType::Fist;
            sound = SfxEnum::Getpow;
            break;

        case SpriteNum::Pins:
            if (!player->givePower(PowerType::Invisibility))
                return;
            player->message = GOTINVIS;
            sound = SfxEnum::Getpow;
            break;

        case SpriteNum::Suit:
            if (!player->givePower(PowerType::IronFeet))
                return;
            player->message = GOTSUIT;
            sound = SfxEnum::Getpow;
            break;

        case SpriteNum::Pmap:
            if (!player->givePower(PowerType::AllMap))
                return;
            player->message = GOTMAP;
            sound = SfxEnum::Getpow;
            break;

        case SpriteNum::Pvis:
            if (!player->givePower(PowerType::Infrared))
                return;
            player->message = GOTVISOR;
            sound = SfxEnum::Getpow;
            break;

            // ammo
        case SpriteNum::Clip:
            if (hasFlag(special.flags, MobjFlag::Dropped))
            {
                if (!player->giveAmmo(AmmoType::Clip, 0))
                    return;
            }
            else
            {
                if (!player->giveAmmo(AmmoType::Clip, 1))
                    return;
            }
            player->message = GOTCLIP;
            break;

        case SpriteNum::Ammo:
            if (!player->giveAmmo(AmmoType::Clip, 5))
                return;
            player->message = GOTCLIPBOX;
            break;

        case SpriteNum::Rock:
            if (!player->giveAmmo(AmmoType::Misl, 1))
                return;
            player->message = GOTROCKET;
            break;

        case SpriteNum::Brok:
            if (!player->giveAmmo(AmmoType::Misl, 5))
                return;
            player->message = GOTROCKBOX;
            break;

        case SpriteNum::Cell:
            if (!player->giveAmmo(AmmoType::Cell, 1))
                return;
            player->message = GOTCELL;
            break;

        case SpriteNum::Celp:
            if (!player->giveAmmo(AmmoType::Cell, 5))
                return;
            player->message = GOTCELLBOX;
            break;

        case SpriteNum::Shel:
            if (!player->giveAmmo(AmmoType::Shell, 1))
                return;
            player->message = GOTSHELLS;
            break;

        case SpriteNum::Sbox:
            if (!player->giveAmmo(AmmoType::Shell, 5))
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
                player->giveAmmo(static_cast<AmmoType>(i), 1);
            player->message = GOTBACKPACK;
            break;

            // weapons
        case SpriteNum::Bfug:
            if (!player->giveWeapon(WeaponType::Bfg, false))
                return;
            player->message = GOTBFG9000;
            sound = SfxEnum::Wpnup;
            break;

        case SpriteNum::Mgun:
            if (!player->giveWeapon(WeaponType::Chaingun,
                                    hasFlag(special.flags, MobjFlag::Dropped)))
                return;
            player->message = GOTCHAINGUN;
            sound = SfxEnum::Wpnup;
            break;

        case SpriteNum::Csaw:
            if (!player->giveWeapon(WeaponType::Chainsaw, false))
                return;
            player->message = GOTCHAINSAW;
            sound = SfxEnum::Wpnup;
            break;

        case SpriteNum::Laun:
            if (!player->giveWeapon(WeaponType::Missile, false))
                return;
            player->message = GOTLAUNCHER;
            sound = SfxEnum::Wpnup;
            break;

        case SpriteNum::Plas:
            if (!player->giveWeapon(WeaponType::Plasma, false))
                return;
            player->message = GOTPLASMA;
            sound = SfxEnum::Wpnup;
            break;

        case SpriteNum::Shot:
            if (!player->giveWeapon(WeaponType::Shotgun,
                                    hasFlag(special.flags, MobjFlag::Dropped)))
                return;
            player->message = GOTSHOTGUN;
            sound = SfxEnum::Wpnup;
            break;

        case SpriteNum::Sgn2:
            if (!player->giveWeapon(WeaponType::SuperShotgun,
                                    hasFlag(special.flags, MobjFlag::Dropped)))
                return;
            player->message = GOTSHOTGUN2;
            sound = SfxEnum::Wpnup;
            break;

        default:
            fatalError("P_SpecialThing: Unknown gettable thing");
    }

    if (hasFlag(special.flags, MobjFlag::CountItem))
        player->itemcount++;
    special.remove();
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

    target.flags = withoutFlags(
        target.flags, MobjFlag::Shootable, MobjFlag::Float, MobjFlag::SkullFly);

    if (target.type != MobjType::Skull)
        target.flags = withoutFlags(target.flags, MobjFlag::NoGravity);

    target.flags = withFlags(target.flags, MobjFlag::Corpse, MobjFlag::DropOff);
    target.height >>= 2;

    auto& players_ = playerState();

    if (source && source->player)
    {
        // count for intermission
        if (hasFlag(target.flags, MobjFlag::CountKill))
            source->player->killcount++;

        if (target.player)
            source->player->frags[target.player - players_.players.data()]++;
    }
    else if (!gameSession().netgame && (hasFlag(target.flags, MobjFlag::CountKill)))
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

        target.flags = withoutFlags(target.flags, MobjFlag::Solid);
        target.player->playerstate = PlayerLifeState::Dead;
        target.player->dropWeapon();

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
        target.setState(target.info->xdeathstate);
    }
    else
        target.setState(static_cast<StateNum>(target.info->deathstate));
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
    mo->flags = withFlags(mo->flags, MobjFlag::Dropped); // special versions of items
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
void Mobj::damage(Mobj* inflictor, Mobj* source, int damage)
{
    Angle ang {};
    int saved;

    if (!(hasFlag(flags, MobjFlag::Shootable)))
        return; // shouldn't happen...

    if (health <= 0)
        return;

    if (hasFlag(flags, MobjFlag::SkullFly))
    {
        momx = momy = momz = Fixed {};
    }

    Player* playerToUse = player;
    if (playerToUse && gameSession().gameskill == Skill::Baby)
        damage >>= 1; // take half damage in trainer mode

    // Some close combat weapons should not
    // inflict thrust and push the victim out of reach,
    // thus kick away unless using the chainsaw.
    if (inflictor && !(hasFlag(flags, MobjFlag::NoClip))
        && (!source || !source->player
            || source->player->readyweapon != WeaponType::Chainsaw))
    {
        ang = pointToAngle2(inflictor->x, inflictor->y, x, y);

        Fixed thrust = damage * (FRACUNIT >> 3) * 100 / info->mass;

        // make fall forwards sometimes
        if (damage < 40 && damage > health && z - inflictor->z > 64 * FRACUNIT
            && (randomness().forPlay() & 1))
        {
            ang += ang180;
            thrust = thrust * 4;
        }

        const auto angFine = ang.fineIndex();
        momx += FixedMul(thrust, finecosine()[angFine]);
        momy += FixedMul(thrust, finesine()[angFine]);
    }

    // player specific
    if (playerToUse)
    {
        // end of game hell hack
        if (subsector->sector->special == 11 && damage >= health)
        {
            damage = health - 1;
        }

        // Below certain threshold,
        // ignore damage in GOD mode, or with INVUL power.
        if (damage < 1000
            && ((hasFlag(playerToUse->cheats, CheatFlag::GodMode))
                || playerToUse->powers[toIndex(PowerType::Invulnerability)]))
        {
            return;
        }

        if (playerToUse->armortype)
        {
            if (playerToUse->armortype == 1)
                saved = damage / 3;
            else
                saved = damage / 2;

            if (playerToUse->armorpoints <= saved)
            {
                // armor is used up
                saved = playerToUse->armorpoints;
                playerToUse->armortype = 0;
            }
            playerToUse->armorpoints -= saved;
            damage -= saved;
        }
        playerToUse->health -= damage; // mirror mobj health here for Dave
        if (playerToUse->health < 0)
            playerToUse->health = 0;

        playerToUse->attacker = source;
        playerToUse->damagecount += damage; // add damage after armor / invuln

        if (playerToUse->damagecount > 100)
            playerToUse->damagecount = 100; // teleport stomp does 10k points...

        int temp = damage < 100 ? damage : 100;

        const auto& players_ = playerState();

        if (playerToUse == &players_.players[players_.consoleplayer])
            tactileFeedback(40, 10, 40 + temp * 2);
    }

    // do the damage
    health -= damage;
    if (health <= 0)
    {
        killMobj(source, *this);
        return;
    }

    if ((randomness().forPlay() < info->painchance)
        && !(hasFlag(flags, MobjFlag::SkullFly)))
    {
        flags = withFlags(flags, MobjFlag::JustHit); // fight back!

        setState(static_cast<StateNum>(info->painstate));
    }

    reactiontime = 0; // we're awake now...

    if ((!threshold || type == MobjType::Vile) && source && source != this
        && source->type != MobjType::Vile)
    {
        // if not intent on another player,
        // chase after this one
        target = source;
        threshold = BASETHRESHOLD;
        if (state == &states()[toIndex(info->spawnstate)]
            && info->seestate != StateNum::Null)
            setState(static_cast<StateNum>(info->seestate));
    }
}
} // namespace Doom
