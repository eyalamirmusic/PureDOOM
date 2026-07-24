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
// Rewritten into namespace Doom out of vanilla p_mobj. The per-tic mobj thinker is
// Mobj::tick() (Thinkers/Mobj.cpp) now, dispatched virtually - p_saveg and the sim
// probe identify a mobj by its kind() rather than by comparing a stored function
// pointer, so nothing here holds the thinker's address. The state driver, movement
// steps, removal and missile spawning it drives (setState / xyMovement / zMovement /
// nightmareRespawn / remove / spawnMissile / ...) are Mobj methods, declared on the
// struct in Thinkers/Mobj.h. The spawn* factories stay free functions here (they
// have no mobj to be a method of).
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

// Defined in g_game (reset a player's state on respawn) and in Clip (the shot range,
// read by xyMovement's melee check).
void Doom::playerReborn(int player);

namespace Doom
{

constexpr Fixed STOPSPEED {0x1000};
constexpr Fixed FRICTION {0xe800};

bool Mobj::setState(StateNum stateToUse)
{
    do
    {
        if (stateToUse == StateNum::Null)
        {
            state = nullptr; // was (State*) S_NULL, i.e. a null pointer
            remove();
            return false;
        }

        State* st = &states()[toIndex(stateToUse)];
        state = st;
        tics = st->tics;
        sprite = st->sprite;
        frame = st->frame;

        // Modified handling.
        // Call action functions when the state is set.
        if (st->action.mobj)
            (this->*(st->action.mobj))();

        stateToUse = st->nextstate;
    } while (!tics);

    return true;
}

//
// explodeMissile
//
void Mobj::explodeMissile()
{
    momx = momy = momz = Fixed {};

    setState(static_cast<StateNum>(mobjinfo()[toIndex(type)].deathstate));

    tics -= randomness().forPlay() & 3;

    if (tics < 1)
        tics = 1;

    flags = withoutFlags(flags, MobjFlag::Missile);

    if (info->deathsound != SfxEnum::None)
        startSound(this, info->deathsound);
}

//
// xyMovement
//
void Mobj::xyMovement()
{
    Fixed ptryx;
    Fixed ptryy;

    auto& c = clipping();

    if (!momx && !momy)
    {
        if (hasFlag(flags, MobjFlag::SkullFly))
        {
            // the skull slammed into something
            flags = withoutFlags(flags, MobjFlag::SkullFly);
            momx = momy = momz = Fixed {};

            setState(static_cast<StateNum>(info->spawnstate));
        }
        return;
    }

    Player* playerToUse = player;

    if (momx > MAXMOVE)
        momx = MAXMOVE;
    else if (momx < -MAXMOVE)
        momx = -MAXMOVE;

    if (momy > MAXMOVE)
        momy = MAXMOVE;
    else if (momy < -MAXMOVE)
        momy = -MAXMOVE;

    Fixed xmove = momx;
    Fixed ymove = momy;

    do
    {
        if (xmove > MAXMOVE / 2 || ymove > MAXMOVE / 2)
        {
            ptryx = x + xmove / 2;
            ptryy = y + ymove / 2;
            xmove >>= 1;
            ymove >>= 1;
        }
        else
        {
            ptryx = x + xmove;
            ptryy = y + ymove;
            xmove = ymove = Fixed {};
        }

        if (!tryMove(ptryx, ptryy))
        {
            // blocked move
            if (playerToUse)
            { // try to slide along it
                slideMove();
            }
            else if (hasFlag(flags, MobjFlag::Missile))
            {
                // explode a missile
                if (c.ceilingline && c.ceilingline->backsector
                    && c.ceilingline->backsector->ceilingpic
                           == skyState().skyflatnum)
                {
                    // Hack to prevent missiles exploding
                    // against the sky.
                    // Does not handle sky floors.
                    remove();
                    return;
                }
                explodeMissile();
            }
            else
                momx = momy = Fixed {};
        }
    } while (xmove || ymove);

    // slow down
    if (playerToUse && hasFlag(playerToUse->cheats, CheatFlag::NoMomentum))
    {
        // debug option for no sliding at all
        momx = momy = Fixed {};
        return;
    }

    if (hasFlag(flags, MobjFlag::Missile, MobjFlag::SkullFly))
        return; // no friction for missiles ever

    if (z > floorz)
        return; // no friction when airborne

    if (hasFlag(flags, MobjFlag::Corpse))
    {
        // do not stop sliding
        //  if halfway off a step with some momentum
        if (momx > FRACUNIT / 4 || momx < -FRACUNIT / 4 || momy > FRACUNIT / 4
            || momy < -FRACUNIT / 4)
        {
            if (floorz != subsector->sector->floorheight)
                return;
        }
    }

    if (momx > -STOPSPEED && momx < STOPSPEED && momy > -STOPSPEED
        && momy < STOPSPEED
        && (!playerToUse
            || (playerToUse->cmd.forwardmove == 0
                && playerToUse->cmd.sidemove == 0)))
    {
        // if in a walking frame, stop moving
        if (playerToUse
            && static_cast<unsigned>((playerToUse->mo->state - states())
                                     - toIndex(StateNum::PlayRun1))
                   < 4)
            playerToUse->mo->setState(StateNum::Play);

        momx = Fixed {};
        momy = Fixed {};
    }
    else
    {
        momx = FixedMul(momx, FRICTION);
        momy = FixedMul(momy, FRICTION);
    }
}

//
// zMovement
//
void Mobj::zMovement()
{
    // check for smooth step up
    if (player && z < floorz)
    {
        player->viewheight -= floorz - z;

        player->deltaviewheight = (VIEWHEIGHT - player->viewheight) >> 3;
    }

    // adjust height
    z += momz;

    if (hasFlag(flags, MobjFlag::Float) && target)
    {
        // float down towards target if too close
        if (!(hasFlag(flags, MobjFlag::SkullFly))
            && !(hasFlag(flags, MobjFlag::InFloat)))
        {
            Fixed dist = approxDistance(x - target->x, y - target->y);

            Fixed delta = (target->z + (height >> 1)) - z;

            if (delta.isNegative() && dist < -(delta * 3))
                z -= FLOATSPEED;
            else if (delta.isPositive() && dist < (delta * 3))
                z += FLOATSPEED;
        }
    }

    // clip movement
    if (z <= floorz)
    {
        // hit the floor

        // Note (id):
        //  somebody left this after the setting momz to 0,
        //  kinda useless there.
        if (hasFlag(flags, MobjFlag::SkullFly))
        {
            // the skull slammed into something
            momz = -momz;
        }

        if (momz.isNegative())
        {
            if (player && momz < -GRAVITY * 8)
            {
                // Squat down.
                // Decrease viewheight for a moment
                // after hitting the ground (hard),
                // and utter appropriate sound.
                player->deltaviewheight = momz >> 3;
                startSound(this, SfxEnum::Oof);
            }
            momz = Fixed {};
        }
        z = floorz;

        if ((hasFlag(flags, MobjFlag::Missile))
            && !(hasFlag(flags, MobjFlag::NoClip)))
        {
            explodeMissile();
            return;
        }
    }
    else if (!(hasFlag(flags, MobjFlag::NoGravity)))
    {
        if (momz.isZero())
            momz = -GRAVITY * 2;
        else
            momz -= GRAVITY;
    }

    if (z + height > ceilingz)
    {
        // hit the ceiling
        if (momz.isPositive())
            momz = Fixed {};
        {
            z = ceilingz - height;
        }

        if (hasFlag(flags, MobjFlag::SkullFly))
        { // the skull slammed into something
            momz = -momz;
        }

        if ((hasFlag(flags, MobjFlag::Missile))
            && !(hasFlag(flags, MobjFlag::NoClip)))
        {
            explodeMissile();
            return;
        }
    }
}

//
// nightmareRespawn
//
void Mobj::nightmareRespawn()
{
    Fixed zToUse;

    Fixed xToUse = Fixed::fromInt(spawnpoint.x);
    Fixed yToUse = Fixed::fromInt(spawnpoint.y);

    // somthing is occupying it's position?
    if (!checkPosition(xToUse, yToUse))
        return; // no respwan

    // spawn a teleport fog at old spot
    // because of removal of the body?
    Mobj* mo =
        spawnMobj(xToUse, yToUse, subsector->sector->floorheight, MobjType::Tfog);
    // initiate teleport sound
    startSound(mo, SfxEnum::Telept);

    // spawn a teleport fog at the new spot
    SubSector* ss = pointInSubsector(xToUse, yToUse);

    mo = spawnMobj(xToUse, yToUse, ss->sector->floorheight, MobjType::Tfog);

    startSound(mo, SfxEnum::Telept);

    // spawn the new monster
    MapThing* mthing = &spawnpoint;

    // spawn it
    if (hasFlag(info->flags, MobjFlag::SpawnCeiling))
        zToUse = ONCEILINGZ;
    else
        zToUse = ONFLOORZ;

    // inherit attributes from deceased one
    mo = spawnMobj(xToUse, yToUse, zToUse, type);
    mo->spawnpoint = spawnpoint;
    mo->angle = ang45 * (mthing->angle / 45);

    if (mthing->options & MTF_AMBUSH)
        mo->flags = withFlags(mo->flags, MobjFlag::Ambush);

    mo->reactiontime = 18;

    // remove the old monster,
    remove();
}

//
// spawnMobj
//
Mobj* spawnMobj(Fixed x, Fixed y, Fixed z, MobjType type)
{
    Mobj* mobj = new (levelAlloc(sizeof(*mobj))) Mobj {};
    MobjInfo* info = &mobjinfo()[toIndex(type)];

    mobj->type = type;
    mobj->info = info;
    mobj->x = x;
    mobj->y = y;
    mobj->radius = info->radius;
    mobj->height = info->height;
    mobj->flags = info->flags;
    mobj->health = info->spawnhealth;

    if (gameSession().gameskill != Skill::Nightmare)
        mobj->reactiontime = info->reactiontime;

    mobj->lastlook = randomness().forPlay() % MAXPLAYERS;
    // do not set the state with setMobjState,
    // because action routines can not be called yet
    State* st = &states()[toIndex(info->spawnstate)];

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

    addThinker(*mobj);

    return mobj;
}

//
// removeMobj
//
void Mobj::remove()
{
    if ((hasFlag(flags, MobjFlag::Special)) && !(hasFlag(flags, MobjFlag::Dropped))
        && (type != MobjType::Inv) && (type != MobjType::Ins))
    {
        auto& queue = itemRespawnQueue();

        queue.itemrespawnque[queue.iquehead] = spawnpoint;
        queue.itemrespawntime[queue.iquehead] = levelStats().leveltime;
        queue.iquehead = (queue.iquehead + 1) & (ITEMQUESIZE - 1);

        // lose one off the end?
        if (queue.iquehead == queue.iquetail)
            queue.iquetail = (queue.iquetail + 1) & (ITEMQUESIZE - 1);
    }

    // unlink from sector and block lists
    unsetThingPosition(*this);

    // stop any playing sound
    stopSound(this);

    // free block
    removeThinker(*this);
}

//
// respawnSpecials
//
void respawnSpecials()
{
    Fixed z;

    int i;

    // only respawn items in deathmatch
    if (gameSession().deathmatch != 2)
        return; //

    auto& queue = itemRespawnQueue();

    // nothing left to respawn?
    if (queue.iquehead == queue.iquetail)
        return;

    // wait at least 30 seconds
    if (levelStats().leveltime - queue.itemrespawntime[queue.iquetail] < 30 * 35)
        return;

    MapThing* mthing = &queue.itemrespawnque[queue.iquetail];

    Fixed x = Fixed::fromInt(mthing->x);
    Fixed y = Fixed::fromInt(mthing->y);

    // spawn a teleport fog at the new spot
    SubSector* ss = pointInSubsector(x, y);
    Mobj* mo = spawnMobj(x, y, ss->sector->floorheight, MobjType::Ifog);
    startSound(mo, SfxEnum::Itmbk);

    // find which type to spawn
    for (i = 0; i < numMobjTypes; i++)
    {
        if (mthing->type == mobjinfo()[i].doomednum)
            break;
    }

    // spawn it
    if (hasFlag(mobjinfo()[i].flags, MobjFlag::SpawnCeiling))
        z = ONCEILINGZ;
    else
        z = ONFLOORZ;

    mo = spawnMobj(x, y, z, static_cast<MobjType>(i));
    mo->spawnpoint = *mthing;
    mo->angle = ang45 * (mthing->angle / 45);

    // pull it from the que
    queue.iquetail = (queue.iquetail + 1) & (ITEMQUESIZE - 1);
}

//
// spawnPlayer
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged
// between levels.
//
void spawnPlayer(MapThing& mthing)
{
    auto& players_ = playerState();

    // not playing?
    if (!players_.playeringame[mthing.type - 1])
        return;

    Player* p = &players_.players[mthing.type - 1];

    if (p->playerstate == PlayerLifeState::Reborn)
        playerReborn(mthing.type - 1);

    Fixed x = Fixed::fromInt(mthing.x);
    Fixed y = Fixed::fromInt(mthing.y);
    Fixed z = ONFLOORZ;
    Mobj* mobj = spawnMobj(x, y, z, MobjType::Player);

    // set color translations for player sprites
    if (mthing.type > 1)
        mobj->flags |= (mthing.type - 1) << mobjTranslationShift;

    mobj->angle = ang45 * (mthing.angle / 45);
    mobj->player = p;
    mobj->health = p->health;

    p->mo = mobj;
    p->playerstate = PlayerLifeState::Live;
    p->refire = 0;
    p->message = {};
    p->damagecount = 0;
    p->bonuscount = 0;
    p->extralight = 0;
    p->fixedcolormap = 0;
    p->viewheight = VIEWHEIGHT;

    // setup gun psprite
    p->setupPsprites();

    // give all cards in death match mode
    if (gameSession().deathmatch)
        for (bool& card: p->cards)
            card = true;

    if (mthing.type - 1 == players_.consoleplayer)
    {
        // wake up the status bar
        startStatusBar();
        // wake up the heads up text
        startHud();
    }
}

//
// spawnMapThing
// The fields of the mapthing should
// already be in host byte order.
//
void spawnMapThing(MapThing& mthing)
{
    int i;
    int bit;
    Fixed z;

    auto& spawns = mapSpawns();
    const auto& session = gameSession();

    // count deathmatch start positions
    if (mthing.type == 11)
    {
        // data() + N, not &deathmatchstarts[N]. Both name the one-past-the-end
        // address, and on the raw C array this used to be that was the ordinary
        // way to write it - but subscripting at N is out of range for the
        // Array (std::array) it is now, and MSVC's debug STL asserts on it.
        // The bound stays MAX_DM_STARTS, the same token that sizes the array in
        // MapSpawns.h, so raising it cannot move one without the other.
        if (spawns.deathmatch_p < spawns.deathmatchstarts.data() + MAX_DM_STARTS)
        {
            doom_memcpy(spawns.deathmatch_p, &mthing, sizeof(mthing));
            spawns.deathmatch_p++;
        }
        return;
    }

    // check for players specially
    if (mthing.type <= 4)
    {
        // save spots for respawning in network games
        spawns.playerstarts[mthing.type - 1] = mthing;
        if (!session.deathmatch)
            spawnPlayer(mthing);

        return;
    }

    // check for apropriate skill level
    if (!session.netgame && (mthing.options & 16))
        return;

    if (session.gameskill == Skill::Baby)
        bit = 1;
    else if (session.gameskill == Skill::Nightmare)
        bit = 4;
    else
        bit = 1 << (static_cast<int>(session.gameskill) - 1);

    if (!(mthing.options & bit))
        return;

    // find which type to spawn
    for (i = 0; i < numMobjTypes; i++)
        if (mthing.type == mobjinfo()[i].doomednum)
            break;

    if (i == numMobjTypes)
    {
        //fatalError("Error: spawnMapThing: Unknown type %i at (%i, %i)",
        //        mthing.type,
        //        mthing.x, mthing.y);

        fatalError("Error: spawnMapThing: Unknown type ",
                   mthing.type,
                   " at (",
                   mthing.x,
                   ", ",
                   mthing.y,
                   ")");
    }

    // don't spawn keycards and players in deathmatch
    if (session.deathmatch && hasFlag(mobjinfo()[i].flags, MobjFlag::NotDmatch))
        return;

    // don't spawn any monsters if -nomonsters
    if (launchOptions().nomonsters
        && (i == toIndex(MobjType::Skull)
            || (hasFlag(mobjinfo()[i].flags, MobjFlag::CountKill))))
    {
        return;
    }

    // spawn it
    Fixed x = Fixed::fromInt(mthing.x);
    Fixed y = Fixed::fromInt(mthing.y);

    if (hasFlag(mobjinfo()[i].flags, MobjFlag::SpawnCeiling))
        z = ONCEILINGZ;
    else
        z = ONFLOORZ;

    Mobj* mobj = spawnMobj(x, y, z, static_cast<MobjType>(i));
    mobj->spawnpoint = mthing;

    if (mobj->tics > 0)
        mobj->tics = 1 + (randomness().forPlay() % mobj->tics);
    auto& stats = levelStats();

    if (hasFlag(mobj->flags, MobjFlag::CountKill))
        stats.totalkills++;
    if (hasFlag(mobj->flags, MobjFlag::CountItem))
        stats.totalitems++;

    mobj->angle = ang45 * (mthing.angle / 45);
    if (mthing.options & MTF_AMBUSH)
        mobj->flags = withFlags(mobj->flags, MobjFlag::Ambush);
}

//
// GAME SPAWN FUNCTIONS
//

//
// spawnPuff
//
void spawnPuff(Fixed x, Fixed y, Fixed z)
{
    z += Fixed {(randomness().forPlay() - randomness().forPlay()) << 10};

    Mobj* th = spawnMobj(x, y, z, MobjType::Puff);
    th->momz = FRACUNIT;
    th->tics -= randomness().forPlay() & 3;

    if (th->tics < 1)
        th->tics = 1;

    // don't make punches spark on the wall
    if (clipping().attackrange == MELEERANGE)
        th->setState(StateNum::Puff3);
}

//
// spawnBlood
//
void spawnBlood(Fixed x, Fixed y, Fixed z, int damage)
{
    z += Fixed {(randomness().forPlay() - randomness().forPlay()) << 10};
    Mobj* th = spawnMobj(x, y, z, MobjType::Blood);
    th->momz = FRACUNIT * 2;
    th->tics -= randomness().forPlay() & 3;

    if (th->tics < 1)
        th->tics = 1;

    if (damage <= 12 && damage >= 9)
        th->setState(StateNum::Blood2);
    else if (damage < 9)
        th->setState(StateNum::Blood3);
}

//
// checkMissileSpawn
// Moves the missile forward a bit
//  and possibly explodes it right there.
//
void Mobj::checkMissileSpawn()
{
    tics -= randomness().forPlay() & 3;
    if (tics < 1)
        tics = 1;

    // move a little forward so an angle can
    // be computed if it immediately explodes
    x += (momx >> 1);
    y += (momy >> 1);
    z += (momz >> 1);

    if (!tryMove(x, y))
        explodeMissile();
}

//
// spawnMissile
//
Mobj* Mobj::spawnMissile(Mobj* dest, MobjType typeToUse)
{
    Mobj* th = spawnMobj(x, y, z + 4 * 8 * FRACUNIT, typeToUse);

    if (th->info->seesound != SfxEnum::None)
        startSound(th, th->info->seesound);

    th->target = this; // where it came from
    Angle an = pointToAngle2(x, y, dest->x, dest->y);

    // fuzzy player
    if (hasFlag(dest->flags, MobjFlag::Shadow))
        an += Angle {(unsigned) (randomness().forPlay() - randomness().forPlay())
                     << 20};

    th->angle = an;
    const auto anFine = an.fineIndex();
    th->momx = FixedMul(Fixed {th->info->speed}, finecosine()[anFine]);
    th->momy = FixedMul(Fixed {th->info->speed}, finesine()[anFine]);

    // dist is vanilla's tic count, not a length: the raw distance divided by the
    // missile's raw speed as plain integers, then used as the divisor for momz.
    int dist = approxDistance(dest->x - x, dest->y - y).raw;
    dist = dist / th->info->speed;

    if (dist < 1)
        dist = 1;

    th->momz = (dest->z - z) / dist;
    th->checkMissileSpawn();

    return th;
}

//
// spawnPlayerMissile
// Tries to aim at a nearby monster
//
void Mobj::spawnPlayerMissile(MobjType typeToUse)
{
    // see which target is to be aimed at
    Angle an = angle;
    auto aim = aimLineAttack(this, an, 16 * 64 * FRACUNIT);
    Fixed slope = aim.slope;

    if (!aim.target)
    {
        an += Angle {1u << 26};
        aim = aimLineAttack(this, an, 16 * 64 * FRACUNIT);
        slope = aim.slope;

        if (!aim.target)
        {
            an -= Angle {2u << 26};
            aim = aimLineAttack(this, an, 16 * 64 * FRACUNIT);
            slope = aim.slope;
        }

        if (!aim.target)
        {
            an = angle;
            slope = Fixed {};
        }
    }

    Fixed xToUse = x;
    Fixed yToUse = y;
    Fixed zToUse = z + 4 * 8 * FRACUNIT;

    Mobj* th = spawnMobj(xToUse, yToUse, zToUse, typeToUse);

    if (th->info->seesound != SfxEnum::None)
        startSound(th, th->info->seesound);

    th->target = this;
    th->angle = an;
    th->momx = FixedMul(Fixed {th->info->speed}, finecosine()[an.fineIndex()]);
    th->momy = FixedMul(Fixed {th->info->speed}, finesine()[an.fineIndex()]);
    th->momz = FixedMul(Fixed {th->info->speed}, slope);

    th->checkMissileSpawn();
}
} // namespace Doom
