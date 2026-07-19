// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Moving object handling. Spawn functions, the mobj thinker, missiles.
//
// Rewritten into namespace Doom out of vanilla p_mobj; p_mobj.cpp keeps the vanilla
// names as shims and the item-respawn queue globals. Doom::mobjThinker in particular
// stays a global shim: p_saveg and the sim probe identify mobjs by comparing a
// thinker's function pointer to it, so spawnMobj below stores that global address,
// not this file's mobjThinker.
//
//-----------------------------------------------------------------------------

#include "../Host/Platform.h"

#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"
#include "../UI/Hud.h"
#include "Random.h"
#include "SimDefs.h"
#include "../Game/SoundData.h"
#include "../UI/StatusBarTypes.h"

#include "../Game/GameSession.h"
#include "../Game/LaunchOptions.h"
#include "../Game/LevelStats.h"
#include "../Game/MapSpawns.h"
#include "../Game/PlayerState.h"
#include "../Game/SkyState.h"
#include "Clip.h"
#include "Mobj.h"
#include "Tick.h" // levelAlloc / levelFree / freeLevelAllocations
#include "../UI/Hud.h"

#include "../UI/StatusBar.h"
#include <new>

#include "../Game/Game.h"
#include "../Game/Sound.h"
#include "../Host/System.h"
#include "../Render/Main.h"
#include "MapAction.h"
#include "MapUtil.h"
#include "Movement.h"
#include "Weapon.h"
#include "Random.h"
#include "ItemRespawnQueue.h"
#define STOPSPEED (Doom::Fixed {0x1000})
#define FRICTION (Doom::Fixed {0xe800})

// Defined in g_game (reset a player's state on respawn) and in Clip (the shot range,
// read by xyMovement's melee check).
void Doom::playerReborn(int player);

namespace Doom
{

doom_boolean setMobjState(Mobj* mobj, StateNum state)
{
    State* st;

    do
    {
        if (state == S_NULL)
        {
            mobj->state = (State*) S_NULL;
            removeMobj(mobj);
            return false;
        }

        st = &states[state];
        mobj->state = st;
        mobj->tics = st->tics;
        mobj->sprite = st->sprite;
        mobj->frame = st->frame;

        // Modified handling.
        // Call action functions when the state is set. The action is stored
        // type-erased; a mobj state carries a (Mobj*) action, cast back from the
        // erased pointer here (a round-trip conversion, hence well-defined).
        if (st->action.fn)
            reinterpret_cast<void (*)(Mobj*)>(st->action.fn)(mobj);

        state = st->nextstate;
    } while (!mobj->tics);

    return true;
}

//
// explodeMissile
//
void explodeMissile(Mobj* mo)
{
    mo->momx = mo->momy = mo->momz = fixed_t {};

    setMobjState(mo, static_cast<StateNum>(mobjinfo[mo->type].deathstate));

    mo->tics -= Doom::randomness().forPlay() & 3;

    if (mo->tics < 1)
        mo->tics = 1;

    mo->flags &= ~MF_MISSILE;

    if (mo->info->deathsound)
        Doom::startSound(mo, mo->info->deathsound);
}

//
// xyMovement
//
void xyMovement(Mobj* mo)
{
    fixed_t ptryx;
    fixed_t ptryy;
    Player* player;
    fixed_t xmove;
    fixed_t ymove;

    auto& c = clip();

    if (!mo->momx && !mo->momy)
    {
        if (mo->flags & MF_SKULLFLY)
        {
            // the skull slammed into something
            mo->flags &= ~MF_SKULLFLY;
            mo->momx = mo->momy = mo->momz = fixed_t {};

            setMobjState(mo, static_cast<StateNum>(mo->info->spawnstate));
        }
        return;
    }

    player = mo->player;

    if (mo->momx > MAXMOVE)
        mo->momx = MAXMOVE;
    else if (mo->momx < -MAXMOVE)
        mo->momx = -MAXMOVE;

    if (mo->momy > MAXMOVE)
        mo->momy = MAXMOVE;
    else if (mo->momy < -MAXMOVE)
        mo->momy = -MAXMOVE;

    xmove = mo->momx;
    ymove = mo->momy;

    do
    {
        if (xmove > MAXMOVE / 2 || ymove > MAXMOVE / 2)
        {
            ptryx = mo->x + xmove / 2;
            ptryy = mo->y + ymove / 2;
            xmove >>= 1;
            ymove >>= 1;
        }
        else
        {
            ptryx = mo->x + xmove;
            ptryy = mo->y + ymove;
            xmove = ymove = fixed_t {};
        }

        if (!Doom::tryMove(mo, ptryx, ptryy))
        {
            // blocked move
            if (mo->player)
            { // try to slide along it
                Doom::slideMove(mo);
            }
            else if (mo->flags & MF_MISSILE)
            {
                // explode a missile
                if (c.ceilingline && c.ceilingline->backsector
                    && c.ceilingline->backsector->ceilingpic
                           == skyState().skyflatnum)
                {
                    // Hack to prevent missiles exploding
                    // against the sky.
                    // Does not handle sky floors.
                    removeMobj(mo);
                    return;
                }
                explodeMissile(mo);
            }
            else
                mo->momx = mo->momy = fixed_t {};
        }
    } while (xmove || ymove);

    // slow down
    if (player && player->cheats & CF_NOMOMENTUM)
    {
        // debug option for no sliding at all
        mo->momx = mo->momy = fixed_t {};
        return;
    }

    if (mo->flags & (MF_MISSILE | MF_SKULLFLY))
        return; // no friction for missiles ever

    if (mo->z > mo->floorz)
        return; // no friction when airborne

    if (mo->flags & MF_CORPSE)
    {
        // do not stop sliding
        //  if halfway off a step with some momentum
        if (mo->momx > FRACUNIT / 4 || mo->momx < -FRACUNIT / 4
            || mo->momy > FRACUNIT / 4 || mo->momy < -FRACUNIT / 4)
        {
            if (mo->floorz != mo->subsector->sector->floorheight)
                return;
        }
    }

    if (mo->momx > -STOPSPEED && mo->momx < STOPSPEED && mo->momy > -STOPSPEED
        && mo->momy < STOPSPEED
        && (!player || (player->cmd.forwardmove == 0 && player->cmd.sidemove == 0)))
    {
        // if in a walking frame, stop moving
        if (player
            && static_cast<unsigned>((player->mo->state - states) - S_PLAY_RUN1) < 4)
            setMobjState(player->mo, S_PLAY);

        mo->momx = fixed_t {};
        mo->momy = fixed_t {};
    }
    else
    {
        mo->momx = FixedMul(mo->momx, FRICTION);
        mo->momy = FixedMul(mo->momy, FRICTION);
    }
}

//
// zMovement
//
void zMovement(Mobj* mo)
{
    fixed_t dist;
    fixed_t delta;

    // check for smooth step up
    if (mo->player && mo->z < mo->floorz)
    {
        mo->player->viewheight -= mo->floorz - mo->z;

        mo->player->deltaviewheight = (VIEWHEIGHT - mo->player->viewheight) >> 3;
    }

    // adjust height
    mo->z += mo->momz;

    if (mo->flags & MF_FLOAT && mo->target)
    {
        // float down towards target if too close
        if (!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
        {
            dist = approxDistance(mo->x - mo->target->x, mo->y - mo->target->y);

            delta = (mo->target->z + (mo->height >> 1)) - mo->z;

            if (delta.isNegative() && dist < -(delta * 3))
                mo->z -= FLOATSPEED;
            else if (delta.isPositive() && dist < (delta * 3))
                mo->z += FLOATSPEED;
        }
    }

    // clip movement
    if (mo->z <= mo->floorz)
    {
        // hit the floor

        // Note (id):
        //  somebody left this after the setting momz to 0,
        //  kinda useless there.
        if (mo->flags & MF_SKULLFLY)
        {
            // the skull slammed into something
            mo->momz = -mo->momz;
        }

        if (mo->momz.isNegative())
        {
            if (mo->player && mo->momz < -GRAVITY * 8)
            {
                // Squat down.
                // Decrease viewheight for a moment
                // after hitting the ground (hard),
                // and utter appropriate sound.
                mo->player->deltaviewheight = mo->momz >> 3;
                Doom::startSound(mo, sfx_oof);
            }
            mo->momz = fixed_t {};
        }
        mo->z = mo->floorz;

        if ((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            explodeMissile(mo);
            return;
        }
    }
    else if (!(mo->flags & MF_NOGRAVITY))
    {
        if (mo->momz.isZero())
            mo->momz = -GRAVITY * 2;
        else
            mo->momz -= GRAVITY;
    }

    if (mo->z + mo->height > mo->ceilingz)
    {
        // hit the ceiling
        if (mo->momz.isPositive())
            mo->momz = fixed_t {};
        {
            mo->z = mo->ceilingz - mo->height;
        }

        if (mo->flags & MF_SKULLFLY)
        { // the skull slammed into something
            mo->momz = -mo->momz;
        }

        if ((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            explodeMissile(mo);
            return;
        }
    }
}

//
// nightmareRespawn
//
void nightmareRespawn(Mobj* mobj)
{
    fixed_t x;
    fixed_t y;
    fixed_t z;
    SubSector* ss;
    Mobj* mo;
    MapThing* mthing;

    x = Doom::Fixed::fromInt(mobj->spawnpoint.x);
    y = Doom::Fixed::fromInt(mobj->spawnpoint.y);

    // somthing is occupying it's position?
    if (!Doom::checkPosition(mobj, x, y))
        return; // no respwan

    // spawn a teleport fog at old spot
    // because of removal of the body?
    mo = spawnMobj(mobj->x, mobj->y, mobj->subsector->sector->floorheight, MT_TFOG);
    // initiate teleport sound
    Doom::startSound(mo, sfx_telept);

    // spawn a teleport fog at the new spot
    ss = Doom::pointInSubsector(x, y);

    mo = spawnMobj(x, y, ss->sector->floorheight, MT_TFOG);

    Doom::startSound(mo, sfx_telept);

    // spawn the new monster
    mthing = &mobj->spawnpoint;

    // spawn it
    if (mobj->info->flags & MF_SPAWNCEILING)
        z = ONCEILINGZ;
    else
        z = ONFLOORZ;

    // inherit attributes from deceased one
    mo = spawnMobj(x, y, z, mobj->type);
    mo->spawnpoint = mobj->spawnpoint;
    mo->angle = ANG45 * (mthing->angle / 45);

    if (mthing->options & MTF_AMBUSH)
        mo->flags |= MF_AMBUSH;

    mo->reactiontime = 18;

    // remove the old monster,
    removeMobj(mobj);
}

//
// mobjThinker
//
void mobjThinker(Mobj* mobj)
{
    // momentum movement
    if (mobj->momx || mobj->momy || (mobj->flags & MF_SKULLFLY))
    {
        xyMovement(mobj);

        // FIXME: decent NOP/0/Nil function pointer please.
        if (mobj->removed)
            return; // mobj was removed
    }
    if ((mobj->z != mobj->floorz) || mobj->momz)
    {
        zMovement(mobj);

        // FIXME: decent NOP/0/Nil function pointer please.
        if (mobj->removed)
            return; // mobj was removed
    }

    // cycle through states,
    // calling action functions at transitions
    if (mobj->tics != -1)
    {
        mobj->tics--;

        // you can cycle through multiple states in a tic
        if (!mobj->tics)
            if (!setMobjState(mobj, mobj->state->nextstate))
                return; // freed itself
    }
    else
    {
        // check for nightmare respawn
        if (!(mobj->flags & MF_COUNTKILL))
            return;

        if (!gameSession().respawnmonsters)
            return;

        mobj->movecount++;

        if (mobj->movecount < 12 * 35)
            return;

        if (levelStats().leveltime & 31)
            return;

        if (Doom::randomness().forPlay() > 4)
            return;

        nightmareRespawn(mobj);
    }
}

//
// spawnMobj
//
Mobj* spawnMobj(fixed_t x, fixed_t y, fixed_t z, MobjType type)
{
    Mobj* mobj;
    State* st;
    MobjInfo* info;

    mobj = new (levelAlloc(sizeof(*mobj))) Mobj {};
    info = &mobjinfo[type];

    mobj->type = type;
    mobj->info = info;
    mobj->x = x;
    mobj->y = y;
    mobj->radius = info->radius;
    mobj->height = info->height;
    mobj->flags = info->flags;
    mobj->health = info->spawnhealth;

    if (gameSession().gameskill != sk_nightmare)
        mobj->reactiontime = info->reactiontime;

    mobj->lastlook = Doom::randomness().forPlay() % MAXPLAYERS;
    // do not set the state with setMobjState,
    // because action routines can not be called yet
    st = &states[info->spawnstate];

    mobj->state = st;
    mobj->tics = st->tics;
    mobj->sprite = st->sprite;
    mobj->frame = st->frame;

    // set subsector and/or block links
    setThingPosition(*mobj);

    mobj->floorz = mobj->subsector->sector->floorheight;
    mobj->ceilingz = mobj->subsector->sector->ceilingheight;

    if (z == ONFLOORZ)
        mobj->z = mobj->floorz;
    else if (z == ONCEILINGZ)
        mobj->z = mobj->ceilingz - mobj->info->height;
    else
        mobj->z = z;

    Doom::addThinker(mobj);

    return mobj;
}

//
// removeMobj
//
void removeMobj(Mobj* mobj)
{
    if ((mobj->flags & MF_SPECIAL) && !(mobj->flags & MF_DROPPED)
        && (mobj->type != MT_INV) && (mobj->type != MT_INS))
    {
        auto& queue = itemRespawnQueue();

        queue.itemrespawnque[queue.iquehead] = mobj->spawnpoint;
        queue.itemrespawntime[queue.iquehead] = levelStats().leveltime;
        queue.iquehead = (queue.iquehead + 1) & (ITEMQUESIZE - 1);

        // lose one off the end?
        if (queue.iquehead == queue.iquetail)
            queue.iquetail = (queue.iquetail + 1) & (ITEMQUESIZE - 1);
    }

    // unlink from sector and block lists
    unsetThingPosition(*mobj);

    // stop any playing sound
    Doom::stopSound(mobj);

    // free block
    Doom::removeThinker(reinterpret_cast<Doom::Thinker*>(mobj));
}

//
// respawnSpecials
//
void respawnSpecials()
{
    fixed_t x;
    fixed_t y;
    fixed_t z;

    SubSector* ss;
    Mobj* mo;
    MapThing* mthing;

    int i;

    // only respawn items in deathmatch
    if (gameSession().deathmatch != 2)
        return; //

    auto& queue = itemRespawnQueue();

    // nothing left to respawn?
    if (queue.iquehead == queue.iquetail)
        return;

    // wait at least 30 seconds
    if (levelStats().leveltime - queue.itemrespawntime[queue.iquetail]
        < 30 * 35)
        return;

    mthing = &queue.itemrespawnque[queue.iquetail];

    x = Doom::Fixed::fromInt(mthing->x);
    y = Doom::Fixed::fromInt(mthing->y);

    // spawn a teleport fog at the new spot
    ss = Doom::pointInSubsector(x, y);
    mo = spawnMobj(x, y, ss->sector->floorheight, MT_IFOG);
    Doom::startSound(mo, sfx_itmbk);

    // find which type to spawn
    for (i = 0; i < NUMMOBJTYPES; i++)
    {
        if (mthing->type == mobjinfo[i].doomednum)
            break;
    }

    // spawn it
    if (mobjinfo[i].flags & MF_SPAWNCEILING)
        z = ONCEILINGZ;
    else
        z = ONFLOORZ;

    mo = spawnMobj(x, y, z, static_cast<MobjType>(i));
    mo->spawnpoint = *mthing;
    mo->angle = ANG45 * (mthing->angle / 45);

    // pull it from the que
    queue.iquetail = (queue.iquetail + 1) & (ITEMQUESIZE - 1);
}

//
// spawnPlayer
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged
// between levels.
//
void spawnPlayer(MapThing* mthing)
{
    Player* p;
    fixed_t x;
    fixed_t y;
    fixed_t z;

    Mobj* mobj;

    auto& players_ = playerState();

    // not playing?
    if (!players_.playeringame[mthing->type - 1])
        return;

    p = &players_.players[mthing->type - 1];

    if (p->playerstate == PST_REBORN)
        Doom::playerReborn(mthing->type - 1);

    x = Doom::Fixed::fromInt(mthing->x);
    y = Doom::Fixed::fromInt(mthing->y);
    z = ONFLOORZ;
    mobj = spawnMobj(x, y, z, MT_PLAYER);

    // set color translations for player sprites
    if (mthing->type > 1)
        mobj->flags |= (mthing->type - 1) << MF_TRANSSHIFT;

    mobj->angle = ANG45 * (mthing->angle / 45);
    mobj->player = p;
    mobj->health = p->health;

    p->mo = mobj;
    p->playerstate = PST_LIVE;
    p->refire = 0;
    p->message = nullptr;
    p->damagecount = 0;
    p->bonuscount = 0;
    p->extralight = 0;
    p->fixedcolormap = 0;
    p->viewheight = VIEWHEIGHT;

    // setup gun psprite
    setupPsprites(*p);

    // give all cards in death match mode
    if (gameSession().deathmatch)
        for (int i = 0; i < NUMCARDS; i++)
            p->cards[i] = true;

    if (mthing->type - 1 == players_.consoleplayer)
    {
        // wake up the status bar
        Doom::startStatusBar();
        // wake up the heads up text
        Doom::startHud();
    }
}

//
// spawnMapThing
// The fields of the mapthing should
// already be in host byte order.
//
void spawnMapThing(MapThing* mthing)
{
    int i;
    int bit;
    Mobj* mobj;
    fixed_t x;
    fixed_t y;
    fixed_t z;

    auto& spawns = mapSpawns();
    const auto& session = gameSession();

    // count deathmatch start positions
    if (mthing->type == 11)
    {
        if (spawns.deathmatch_p < &spawns.deathmatchstarts[10])
        {
            doom_memcpy(spawns.deathmatch_p, mthing, sizeof(*mthing));
            spawns.deathmatch_p++;
        }
        return;
    }

    // check for players specially
    if (mthing->type <= 4)
    {
        // save spots for respawning in network games
        spawns.playerstarts[mthing->type - 1] = *mthing;
        if (!session.deathmatch)
            spawnPlayer(mthing);

        return;
    }

    // check for apropriate skill level
    if (!session.netgame && (mthing->options & 16))
        return;

    if (session.gameskill == sk_baby)
        bit = 1;
    else if (session.gameskill == sk_nightmare)
        bit = 4;
    else
        bit = 1 << (session.gameskill - 1);

    if (!(mthing->options & bit))
        return;

    // find which type to spawn
    for (i = 0; i < NUMMOBJTYPES; i++)
        if (mthing->type == mobjinfo[i].doomednum)
            break;

    if (i == NUMMOBJTYPES)
    {
        //fatalError("Error: spawnMapThing: Unknown type %i at (%i, %i)",
        //        mthing->type,
        //        mthing->x, mthing->y);

        doom_strcpy(error_buf, "Error: spawnMapThing: Unknown type ");
        doom_concat(error_buf, doom_itoa(mthing->type, 10));
        doom_concat(error_buf, " at (");
        doom_concat(error_buf, doom_itoa(mthing->x, 10));
        doom_concat(error_buf, ", ");
        doom_concat(error_buf, doom_itoa(mthing->y, 10));
        doom_concat(error_buf, ")");
        fatalError(error_buf);
    }

    // don't spawn keycards and players in deathmatch
    if (session.deathmatch && mobjinfo[i].flags & MF_NOTDMATCH)
        return;

    // don't spawn any monsters if -nomonsters
    if (launchOptions().nomonsters
        && (i == MT_SKULL || (mobjinfo[i].flags & MF_COUNTKILL)))
    {
        return;
    }

    // spawn it
    x = Doom::Fixed::fromInt(mthing->x);
    y = Doom::Fixed::fromInt(mthing->y);

    if (mobjinfo[i].flags & MF_SPAWNCEILING)
        z = ONCEILINGZ;
    else
        z = ONFLOORZ;

    mobj = spawnMobj(x, y, z, static_cast<MobjType>(i));
    mobj->spawnpoint = *mthing;

    if (mobj->tics > 0)
        mobj->tics = 1 + (Doom::randomness().forPlay() % mobj->tics);
    auto& stats = levelStats();

    if (mobj->flags & MF_COUNTKILL)
        stats.totalkills++;
    if (mobj->flags & MF_COUNTITEM)
        stats.totalitems++;

    mobj->angle = ANG45 * (mthing->angle / 45);
    if (mthing->options & MTF_AMBUSH)
        mobj->flags |= MF_AMBUSH;
}

//
// GAME SPAWN FUNCTIONS
//

//
// spawnPuff
//
void spawnPuff(fixed_t x, fixed_t y, fixed_t z)
{
    Mobj* th;

    z += Doom::Fixed {(Doom::randomness().forPlay() - Doom::randomness().forPlay())
                      << 10};

    th = spawnMobj(x, y, z, MT_PUFF);
    th->momz = FRACUNIT;
    th->tics -= Doom::randomness().forPlay() & 3;

    if (th->tics < 1)
        th->tics = 1;

    // don't make punches spark on the wall
    if (clip().attackrange == MELEERANGE)
        setMobjState(th, S_PUFF3);
}

//
// spawnBlood
//
void spawnBlood(fixed_t x, fixed_t y, fixed_t z, int damage)
{
    Mobj* th;

    z += Doom::Fixed {(Doom::randomness().forPlay() - Doom::randomness().forPlay())
                      << 10};
    th = spawnMobj(x, y, z, MT_BLOOD);
    th->momz = FRACUNIT * 2;
    th->tics -= Doom::randomness().forPlay() & 3;

    if (th->tics < 1)
        th->tics = 1;

    if (damage <= 12 && damage >= 9)
        setMobjState(th, S_BLOOD2);
    else if (damage < 9)
        setMobjState(th, S_BLOOD3);
}

//
// checkMissileSpawn
// Moves the missile forward a bit
//  and possibly explodes it right there.
//
void checkMissileSpawn(Mobj* th)
{
    th->tics -= Doom::randomness().forPlay() & 3;
    if (th->tics < 1)
        th->tics = 1;

    // move a little forward so an angle can
    // be computed if it immediately explodes
    th->x += (th->momx >> 1);
    th->y += (th->momy >> 1);
    th->z += (th->momz >> 1);

    if (!Doom::tryMove(th, th->x, th->y))
        explodeMissile(th);
}

//
// spawnMissile
//
Mobj* spawnMissile(Mobj* source, Mobj* dest, MobjType type)
{
    Mobj* th;
    angle_t an;
    int dist;

    th = spawnMobj(source->x, source->y, source->z + 4 * 8 * FRACUNIT, type);

    if (th->info->seesound)
        Doom::startSound(th, th->info->seesound);

    th->target = source; // where it came from
    an = Doom::pointToAngle2(source->x, source->y, dest->x, dest->y);

    // fuzzy player
    if (dest->flags & MF_SHADOW)
        an += (Doom::randomness().forPlay() - Doom::randomness().forPlay()) << 20;

    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    th->momx = FixedMul(Doom::Fixed {th->info->speed}, finecosine[an]);
    th->momy = FixedMul(Doom::Fixed {th->info->speed}, finesine[an]);

    // dist is vanilla's tic count, not a length: the raw distance divided by the
    // missile's raw speed as plain integers, then used as the divisor for momz.
    dist = approxDistance(dest->x - source->x, dest->y - source->y).raw;
    dist = dist / th->info->speed;

    if (dist < 1)
        dist = 1;

    th->momz = (dest->z - source->z) / dist;
    checkMissileSpawn(th);

    return th;
}

//
// spawnPlayerMissile
// Tries to aim at a nearby monster
//
void spawnPlayerMissile(Mobj* source, MobjType type)
{
    Mobj* th;
    angle_t an;

    fixed_t x;
    fixed_t y;
    fixed_t z;
    fixed_t slope;

    auto& c = clip();

    // see which target is to be aimed at
    an = source->angle;
    slope = Doom::aimLineAttack(source, an, 16 * 64 * FRACUNIT);

    if (!c.linetarget)
    {
        an += 1 << 26;
        slope = Doom::aimLineAttack(source, an, 16 * 64 * FRACUNIT);

        if (!c.linetarget)
        {
            an -= 2 << 26;
            slope = Doom::aimLineAttack(source, an, 16 * 64 * FRACUNIT);
        }

        if (!c.linetarget)
        {
            an = source->angle;
            slope = fixed_t {};
        }
    }

    x = source->x;
    y = source->y;
    z = source->z + 4 * 8 * FRACUNIT;

    th = spawnMobj(x, y, z, type);

    if (th->info->seesound)
        Doom::startSound(th, th->info->seesound);

    th->target = source;
    th->angle = an;
    th->momx =
        FixedMul(Doom::Fixed {th->info->speed}, finecosine[an >> ANGLETOFINESHIFT]);
    th->momy =
        FixedMul(Doom::Fixed {th->info->speed}, finesine[an >> ANGLETOFINESHIFT]);
    th->momz = FixedMul(Doom::Fixed {th->info->speed}, slope);

    checkMissileSpawn(th);
}
} // namespace Doom
