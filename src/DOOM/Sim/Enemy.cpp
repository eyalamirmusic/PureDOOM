// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Enemy thinking, AI. Action pointers for the states/definitions.
//
// Rewritten into namespace Doom out of vanilla p_enemy; p_enemy.cpp keeps the
// vanilla Doom::noiseAlert name as a shim (Sim/Info.cpp's state table references
// the Doom::Actions:: adapters by address instead, in Sim/Actions.{h,cpp}). The AI
// scratch is file-local; soundtarget stays global (p_saveg archives it), and the
// thinker-identity comparison keeps the global Doom::mobjThinker.
//
//-----------------------------------------------------------------------------

#include "../Host/Platform.h"

#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"
#include "Random.h"
#include "SimDefs.h"
#include "../Game/SoundData.h"

#include "Clip.h"
#include "Enemy.h"
#include "EnemyAI.h"
#include "SoundTarget.h"
#include "ThinkerList.h"
#include "ValidCount.h"

#include "../Containers.h"

#include "../Game/Game.h"
#include "../Game/GameClock.h"
#include "../Game/GameSession.h"
#include "../Game/GameVersion.h"
#include "../Game/LaunchOptions.h"
#include "../Game/PlayerState.h"
#include "../Game/Sound.h"
#include "../Host/System.h"
#include "../Render/Main.h"
#include "Doors.h"
#include "Floors.h"
#include "Interaction.h"
#include "MapAction.h"
#include "MapUtil.h"
#include "Mobj.h"
#include "Movement.h"
#include "Sight.h"
#include "Switches.h"
#include "Weapon.h"
#include "Random.h"

namespace Doom
{
constexpr angle_t FATSPREAD = ang90 / 8;
constexpr fixed_t SKULLSPEED = 20 * FRACUNIT;

// P_NewChaseDir movement LUTs and the transient targets the AI threads through its
// state actions (the vile's corpse, the fat/brain spit targets). All file-local;
// soundtarget alone is shared (now a Doom::SoundTarget Engine member, reached above).
enum class DirType
{
    East,
    NorthEast,
    North,
    NorthWest,
    West,
    SouthWest,
    South,
    SouthEast,
    NoDir,
    NumDirs
};

Array<DirType, 9> opposite = {DirType::West,
                              DirType::SouthWest,
                              DirType::South,
                              DirType::SouthEast,
                              DirType::East,
                              DirType::NorthEast,
                              DirType::North,
                              DirType::NorthWest,
                              DirType::NoDir};

Array<DirType, 4> diags = {
    DirType::NorthWest, DirType::NorthEast, DirType::SouthWest, DirType::SouthEast};

// Raw fixed values: 47000 is FRACUNIT * cos(45) rounded, as vanilla wrote it.
Array<fixed_t, 8> xspeed = {FRACUNIT,
                            fixed_t {47000},
                            fixed_t {},
                            fixed_t {-47000},
                            -FRACUNIT,
                            fixed_t {-47000},
                            fixed_t {},
                            fixed_t {47000}};
Array<fixed_t, 8> yspeed = {fixed_t {},
                            fixed_t {47000},
                            FRACUNIT,
                            fixed_t {47000},
                            fixed_t {},
                            fixed_t {-47000},
                            -FRACUNIT,
                            fixed_t {-47000}};
angle_t TRACEANGLE {0xc000000};
// The monster-AI scratch now lives on the Engine (Sim/EnemyAI.h, moved by the file-scope-statics
// sweep - REFACTOR.md, Step 5). vileCheck, vileChase, brainAwake and brainSpit each hoist
// enemyAI() once and reach its members through it, rather than through file-scope reference
// aliases (REFACTOR.md, Step 9 strand (a)). (The const direction/speed tables above stay
// file-local.)

// Forward declarations so the file's own call order needs no rearranging.
void recursiveSound(Sector* sec, int soundblocks);
void noiseAlert(Mobj& target, Mobj& emmiter);
bool checkMeleeRange(Mobj& actor);
bool checkMissileRange(Mobj& actor);
bool move(Mobj& actor);
bool tryWalk(Mobj& actor);
void newChaseDir(Mobj& actor);
bool lookForPlayers(Mobj& actor, bool allaround);
void keenDie(Mobj& mo);
void look(Mobj& actor);
void chase(Mobj& actor);
void faceTarget(Mobj& actor);
void posAttack(Mobj& actor);
void sPosAttack(Mobj& actor);
void cPosAttack(Mobj& actor);
void cPosRefire(Mobj& actor);
void spidRefire(Mobj& actor);
void bspiAttack(Mobj& actor);
void troopAttack(Mobj& actor);
void sargAttack(Mobj& actor);
void headAttack(Mobj& actor);
void cyberAttack(Mobj& actor);
void bruisAttack(Mobj& actor);
void skelMissile(Mobj& actor);
void tracer(Mobj& actor);
void skelWhoosh(Mobj& actor);
void skelFist(Mobj& actor);
bool vileCheck(Mobj* thing);
void vileChase(Mobj& actor);
void vileStart(Mobj& actor);
void startFire(Mobj& actor);
void fireCrackle(Mobj& actor);
void fire(Mobj& actor);
void vileTarget(Mobj& actor);
void vileAttack(Mobj& actor);
void fatRaise(Mobj& actor);
void fatAttack1(Mobj& actor);
void fatAttack2(Mobj& actor);
void fatAttack3(Mobj& actor);
void skullAttack(Mobj& actor);
void painShootSkull(Mobj& actor, angle_t angle);
void painAttack(Mobj& actor);
void painDie(Mobj& actor);
void scream(Mobj& actor);
void xScream(Mobj& actor);
void pain(Mobj& actor);
void fall(Mobj& actor);
void explode(Mobj& thingy);
void bossDeath(Mobj& mo);
void hoof(Mobj& mo);
void metal(Mobj& mo);
void babyMetal(Mobj& mo);
void openShotgun2(Player& player, PspDef& psp);
void loadShotgun2(Player& player, PspDef& psp);
void closeShotgun2(Player& player, PspDef& psp);
void brainAwake(Mobj& mo);
void brainPain(Mobj& mo);
void brainScream(Mobj& mo);
void brainExplode(Mobj& mo);
void brainDie(Mobj& mo);
void brainSpit(Mobj& mo);
void spawnSound(Mobj& mo);
void spawnFly(Mobj& mo);
void playerScream(Mobj& mo);

void recursiveSound(Sector* sec, int soundblocks)
{
    Sector* other;

    auto& vc = validCount();

    // wake up all monsters in this sector
    if (sec->validcount == vc.validcount && sec->soundtraversed <= soundblocks + 1)
    {
        return; // already flooded
    }

    sec->validcount = vc.validcount;
    sec->soundtraversed = soundblocks + 1;
    sec->soundtarget = soundTarget().soundtarget;

    for (int i = 0; i < sec->linecount; i++)
    {
        Line* check = sec->lines[i];
        if (!(check->flags & ML_TWOSIDED))
            continue;

        updateLineOpening(*check);

        if (!clip().openrange.isPositive())
            continue; // closed door

        if (sides[check->sidenum[0]].sector == sec)
            other = sides[check->sidenum[1]].sector;
        else
            other = sides[check->sidenum[0]].sector;

        if (check->flags & ML_SOUNDBLOCK)
        {
            if (!soundblocks)
                recursiveSound(other, 1);
        }
        else
            recursiveSound(other, soundblocks);
    }
}

//
// noiseAlert
// If a monster yells at a player,
// it will alert other monsters to the player.
//
void noiseAlert(Mobj& target, Mobj& emmiter)
{
    soundTarget().soundtarget = &target;
    validCount().validcount++;
    recursiveSound(emmiter.subsector->sector, 0);
}

//
// checkMeleeRange
//
bool checkMeleeRange(Mobj& actor)
{
    if (!actor.target)
        return false;

    Mobj* pl = actor.target;
    fixed_t dist = approxDistance(pl->x - actor.x, pl->y - actor.y);

    if (dist >= MELEERANGE - 20 * FRACUNIT + pl->info->radius)
        return false;

    if (!checkSight(&actor, actor.target))
        return false;

    return true;
}

//
// checkMissileRange
//
bool checkMissileRange(Mobj& actor)
{
    // Vanilla declares dist fixed_t and then shifts it down to whole units
    // half way through, after which every use of it is a plain integer -
    // compared against 14*64, 196, 200, 160 and P_Random's 0..255.

    if (!checkSight(&actor, actor.target))
        return false;

    if (hasFlag(actor.flags, MobjFlag::JustHit))
    {
        // the target just hit the enemy,
        // so fight back!
        actor.flags = withoutFlags(actor.flags, MobjFlag::JustHit);
        return true;
    }

    if (actor.reactiontime)
        return false; // do not attack yet

    // OPTIMIZE: get this from a global checksight
    fixed_t distance =
        approxDistance(actor.x - actor.target->x, actor.y - actor.target->y)
        - 64 * FRACUNIT;

    if (actor.info->meleestate == StateNum::Null)
        distance -= 128 * FRACUNIT; // no melee attack, so fire more

    int dist = distance.toInt();

    if (actor.type == MobjType::Vile)
    {
        if (dist > 14 * 64)
            return false; // too far away
    }

    if (actor.type == MobjType::Undead)
    {
        if (dist < 196)
            return false; // close for fist attack
        dist >>= 1;
    }

    if (actor.type == MobjType::Cyborg || actor.type == MobjType::Spider
        || actor.type == MobjType::Skull)
    {
        dist >>= 1;
    }

    if (dist > 200)
        dist = 200;

    if (actor.type == MobjType::Cyborg && dist > 160)
        dist = 160;

    if (randomness().forPlay() < dist)
        return false;

    return true;
}

//
// move
// Move in the current direction,
// returns false if the move is blocked.
//
bool move(Mobj& actor)
{
    // warning: 'catch', 'throw', and 'try'
    // are all C++ reserved words

    auto& c = clip();

    if (actor.movedir == toIndex(DirType::NoDir))
        return false;

    if (static_cast<unsigned>(actor.movedir) >= 8)
        fatalError("Error: Weird actor->movedir!");

    fixed_t tryx = actor.x + actor.info->speed * xspeed[actor.movedir];
    fixed_t tryy = actor.y + actor.info->speed * yspeed[actor.movedir];

    bool try_ok = tryMove(actor, tryx, tryy);

    if (!try_ok)
    {
        // open any specials
        if (hasFlag(actor.flags, MobjFlag::Float) && c.floatok)
        {
            // must adjust height
            if (actor.z < c.tmfloorz)
                actor.z += FLOATSPEED;
            else
                actor.z -= FLOATSPEED;

            actor.flags = withFlags(actor.flags, MobjFlag::InFloat);
            return true;
        }

        if (!c.numspechit)
            return false;

        actor.movedir = toIndex(DirType::NoDir);
        bool good = false;
        while (c.numspechit--)
        {
            Line* ld = c.spechit[c.numspechit];
            // if the special is not a door
            // that can be opened,
            // return false
            if (useSpecialLine(actor, *ld, 0))
                good = true;
        }
        return good;
    }
    else
    {
        actor.flags = withoutFlags(actor.flags, MobjFlag::InFloat);
    }

    if (!(hasFlag(actor.flags, MobjFlag::Float)))
        actor.z = actor.floorz;

    return true;
}

//
// TryWalk
// Attempts to move actor on
// in its current (ob->moveangle) direction.
// If blocked by either a wall or an actor
// returns FALSE
// If move is either clear or blocked only by a door,
// returns TRUE and sets...
// If a door is in the way,
// an OpenDoor call is made to start it opening.
//
bool tryWalk(Mobj& actor)
{
    if (!move(actor))
    {
        return false;
    }

    actor.movecount = randomness().forPlay() & 15;
    return true;
}

void newChaseDir(Mobj& actor)
{
    Array<DirType, 3> d;

    int tdir;

    if (!actor.target)
        fatalError("Error: newChaseDir: called with no target");

    DirType olddir = static_cast<DirType>(actor.movedir);
    DirType turnaround = opposite[toIndex(olddir)];

    fixed_t deltax = actor.target->x - actor.x;
    fixed_t deltay = actor.target->y - actor.y;

    if (deltax > 10 * FRACUNIT)
        d[1] = DirType::East;
    else if (deltax < -10 * FRACUNIT)
        d[1] = DirType::West;
    else
        d[1] = DirType::NoDir;

    if (deltay < -10 * FRACUNIT)
        d[2] = DirType::South;
    else if (deltay > 10 * FRACUNIT)
        d[2] = DirType::North;
    else
        d[2] = DirType::NoDir;

    // try direct route
    if (d[1] != DirType::NoDir && d[2] != DirType::NoDir)
    {
        actor.movedir =
            toIndex(diags[(deltay.isNegative() << 1) + deltax.isPositive()]);
        if (actor.movedir != toIndex(turnaround) && tryWalk(actor))
            return;
    }

    // try other directions
    if (randomness().forPlay() > 200 || doom_abs(deltay) > doom_abs(deltax))
    {
        tdir = toIndex(d[1]);
        d[1] = d[2];
        d[2] = static_cast<DirType>(tdir);
    }

    if (d[1] == turnaround)
        d[1] = DirType::NoDir;
    if (d[2] == turnaround)
        d[2] = DirType::NoDir;

    if (d[1] != DirType::NoDir)
    {
        actor.movedir = toIndex(d[1]);
        if (tryWalk(actor))
        {
            // either moved forward or attacked
            return;
        }
    }

    if (d[2] != DirType::NoDir)
    {
        actor.movedir = toIndex(d[2]);

        if (tryWalk(actor))
            return;
    }

    // there is no direct path to the player,
    // so pick another direction.
    if (olddir != DirType::NoDir)
    {
        actor.movedir = toIndex(olddir);

        if (tryWalk(actor))
            return;
    }

    // randomly determine direction of search
    if (randomness().forPlay() & 1)
    {
        for (tdir = toIndex(DirType::East); tdir <= toIndex(DirType::SouthEast);
             tdir++)
        {
            if (tdir != toIndex(turnaround))
            {
                actor.movedir = tdir;

                if (tryWalk(actor))
                    return;
            }
        }
    }
    else
    {
        for (tdir = toIndex(DirType::SouthEast); tdir != toIndex(DirType::East) - 1;
             tdir--)
        {
            if (tdir != toIndex(turnaround))
            {
                actor.movedir = tdir;

                if (tryWalk(actor))
                    return;
            }
        }
    }

    if (turnaround != DirType::NoDir)
    {
        actor.movedir = toIndex(turnaround);
        if (tryWalk(actor))
            return;
    }

    actor.movedir = toIndex(DirType::NoDir); // can not move
}

//
// lookForPlayers
// If allaround is false, only look 180 degrees in front.
// Returns true if a player is targeted.
//
bool lookForPlayers(Mobj& actor, bool allaround)
{
    int c = 0;
    int stop = (actor.lastlook - 1) & 3;

    auto& players_ = playerState();

    for (;; actor.lastlook = (actor.lastlook + 1) & 3)
    {
        if (!players_.playeringame[actor.lastlook])
            continue;

        if (c++ == 2 || actor.lastlook == stop)
        {
            // done looking
            return false;
        }

        Player* player = &players_.players[actor.lastlook];

        if (player->health <= 0)
            continue; // dead

        if (!checkSight(&actor, player->mo))
            continue; // out of sight

        if (!allaround)
        {
            angle_t an =
                pointToAngle2(actor.x, actor.y, player->mo->x, player->mo->y)
                - actor.angle;

            if (an > ang90 && an < ang270)
            {
                fixed_t dist =
                    approxDistance(player->mo->x - actor.x, player->mo->y - actor.y);
                // if real close, react anyway
                if (dist > MELEERANGE)
                    continue; // behind back
            }
        }

        actor.target = player->mo;
        return true;
    }

    return false;
}

//
// keenDie
// DOOM II special, map 32.
// Uses special tag 666.
//
void keenDie(Mobj& mo)
{
    Line junk;

    auto& thinkers = thinkerList();

    fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for (Thinker* th = thinkers.cap.next; th != &thinkers.cap; th = th->next)
    {
        if (th->kind() != ThinkerKind::Mobj || th->removed)
            continue;

        Mobj* mo2 = reinterpret_cast<Mobj*>(th);
        if (mo2 != &mo && mo2->type == mo.type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    junk.tag = 666;
    doDoor(junk, DoorType::Open);
}

//
// ACTION ROUTINES
//

//
// look
// Stay in state until a player is sighted.
//
void look(Mobj& actor)
{
    actor.threshold = 0; // any shot will wake up
    Mobj* targ = actor.subsector->sector->soundtarget;

    if (targ && (hasFlag(targ->flags, MobjFlag::Shootable)))
    {
        actor.target = targ;

        if (hasFlag(actor.flags, MobjFlag::Ambush))
        {
            if (checkSight(&actor, actor.target))
                goto seeyou;
        }
        else
            goto seeyou;
    }

    if (!lookForPlayers(actor, false))
        return;

    // go into chase state
seeyou:
    if (actor.info->seesound != SfxEnum::None)
    {
        SfxEnum sound;

        switch (actor.info->seesound)
        {
            case SfxEnum::Posit1:
            case SfxEnum::Posit2:
            case SfxEnum::Posit3:
                sound = static_cast<SfxEnum>(toIndex(SfxEnum::Posit1)
                                             + randomness().forPlay() % 3);
                break;

            case SfxEnum::Bgsit1:
            case SfxEnum::Bgsit2:
                sound = static_cast<SfxEnum>(toIndex(SfxEnum::Bgsit1)
                                             + randomness().forPlay() % 2);
                break;

            default:
                sound = actor.info->seesound;
                break;
        }

        if (actor.type == MobjType::Spider || actor.type == MobjType::Cyborg)
        {
            // full volume
            startSound(0, sound);
        }
        else
            startSound(&actor, sound);
    }

    setMobjState(actor, static_cast<StateNum>(actor.info->seestate));
}

//
// chase
// Actor has a melee attack,
// so it tries to close as fast as possible
//
void chase(Mobj& actor)
{
    // SIGNED on purpose: vanilla declares this int, so the two tests below are a
    // signed comparison of an angle difference. As an unsigned raw angle, `> 0`
    // would be true for every non-zero value and the `< 0` branch unreachable -
    // which desyncs demo1 at tic 89, monsters turning the wrong way.

    if (actor.reactiontime)
        actor.reactiontime--;

    // modify target threshold
    if (actor.threshold)
    {
        if (!actor.target || actor.target->health <= 0)
        {
            actor.threshold = 0;
        }
        else
            actor.threshold--;
    }

    // turn towards movement direction if not there yet
    if (actor.movedir < 8)
    {
        actor.angle = angle_t {actor.angle.raw & (7u << 29)};
        int delta =
            (int) (actor.angle - angle_t {(unsigned) actor.movedir << 29}).raw;

        if (delta > 0)
            actor.angle -= ang90 / 2u;
        else if (delta < 0)
            actor.angle += ang90 / 2u;
    }

    if (!actor.target || !(hasFlag(actor.target->flags, MobjFlag::Shootable)))
    {
        // look for a new target
        if (lookForPlayers(actor, true))
            return; // got a new target

        setMobjState(actor, static_cast<StateNum>(actor.info->spawnstate));
        return;
    }

    const auto& session = gameSession();
    const auto& opts = launchOptions();

    // do not attack twice in a row
    if (hasFlag(actor.flags, MobjFlag::JustAttacked))
    {
        actor.flags = withoutFlags(actor.flags, MobjFlag::JustAttacked);
        if (session.gameskill != Skill::Nightmare && !opts.fastparm)
            newChaseDir(actor);
        return;
    }

    // check for melee attack
    if (actor.info->meleestate != StateNum::Null && checkMeleeRange(actor))
    {
        if (actor.info->attacksound != SfxEnum::None)
            startSound(&actor, actor.info->attacksound);

        setMobjState(actor, actor.info->meleestate);
        return;
    }

    // check for missile attack
    if (actor.info->missilestate != StateNum::Null)
    {
        if (session.gameskill < Skill::Nightmare && !opts.fastparm
            && actor.movecount)
        {
            goto nomissile;
        }

        if (!checkMissileRange(actor))
            goto nomissile;

        setMobjState(actor, actor.info->missilestate);
        actor.flags = withFlags(actor.flags, MobjFlag::JustAttacked);
        return;
    }

    // ?
nomissile:
    // possibly choose another target
    if (session.netgame && !actor.threshold && !checkSight(&actor, actor.target))
    {
        if (lookForPlayers(actor, true))
            return; // got a new target
    }

    // chase towards player
    if (--actor.movecount < 0 || !move(actor))
    {
        newChaseDir(actor);
    }

    // make active sound
    if (actor.info->activesound != SfxEnum::None && randomness().forPlay() < 3)
    {
        startSound(&actor, actor.info->activesound);
    }
}

//
// faceTarget
//
void faceTarget(Mobj& actor)
{
    if (!actor.target)
        return;

    actor.flags = withoutFlags(actor.flags, MobjFlag::Ambush);

    actor.angle = pointToAngle2(actor.x, actor.y, actor.target->x, actor.target->y);

    if (hasFlag(actor.target->flags, MobjFlag::Shadow))
        actor.angle += angle_t {
            (unsigned) (randomness().forPlay() - randomness().forPlay()) << 21};
}

//
// posAttack
//
void posAttack(Mobj& actor)
{
    if (!actor.target)
        return;

    faceTarget(actor);
    angle_t angle = actor.angle;
    fixed_t slope = aimLineAttack(&actor, angle, MISSILERANGE).slope;

    startSound(&actor, SfxEnum::Pistol);
    angle +=
        angle_t {(unsigned) (randomness().forPlay() - randomness().forPlay()) << 20};
    int damage = ((randomness().forPlay() % 5) + 1) * 3;
    lineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void sPosAttack(Mobj& actor)
{
    angle_t angle {};

    if (!actor.target)
        return;

    startSound(&actor, SfxEnum::Shotgn);
    faceTarget(actor);
    angle_t bangle = actor.angle;
    fixed_t slope = aimLineAttack(&actor, bangle, MISSILERANGE).slope;

    for (int i = 0; i < 3; i++)
    {
        angle =
            bangle
            + angle_t {(unsigned) (randomness().forPlay() - randomness().forPlay())
                       << 20};
        int damage = ((randomness().forPlay() % 5) + 1) * 3;
        lineAttack(actor, angle, MISSILERANGE, slope, damage);
    }
}

void cPosAttack(Mobj& actor)
{
    angle_t angle {};

    if (!actor.target)
        return;

    startSound(&actor, SfxEnum::Shotgn);
    faceTarget(actor);
    angle_t bangle = actor.angle;
    fixed_t slope = aimLineAttack(&actor, bangle, MISSILERANGE).slope;

    angle = bangle
            + angle_t {(unsigned) (randomness().forPlay() - randomness().forPlay())
                       << 20};
    int damage = ((randomness().forPlay() % 5) + 1) * 3;
    lineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void cPosRefire(Mobj& actor)
{
    // keep firing unless target got out of sight
    faceTarget(actor);

    if (randomness().forPlay() < 40)
        return;

    if (!actor.target || actor.target->health <= 0
        || !checkSight(&actor, actor.target))
    {
        setMobjState(actor, static_cast<StateNum>(actor.info->seestate));
    }
}

void spidRefire(Mobj& actor)
{
    // keep firing unless target got out of sight
    faceTarget(actor);

    if (randomness().forPlay() < 10)
        return;

    if (!actor.target || actor.target->health <= 0
        || !checkSight(&actor, actor.target))
    {
        setMobjState(actor, static_cast<StateNum>(actor.info->seestate));
    }
}

void bspiAttack(Mobj& actor)
{
    if (!actor.target)
        return;

    faceTarget(actor);

    // launch a missile
    spawnMissile(actor, actor.target, MobjType::Arachplaz);
}

//
// troopAttack
//
void troopAttack(Mobj& actor)
{
    if (!actor.target)
        return;

    faceTarget(actor);
    if (checkMeleeRange(actor))
    {
        startSound(&actor, SfxEnum::Claw);
        int damage = (randomness().forPlay() % 8 + 1) * 3;
        damageMobj(*actor.target, &actor, &actor, damage);
        return;
    }

    // launch a missile
    spawnMissile(actor, actor.target, MobjType::Troopshot);
}

void sargAttack(Mobj& actor)
{
    if (!actor.target)
        return;

    faceTarget(actor);
    if (checkMeleeRange(actor))
    {
        int damage = ((randomness().forPlay() % 10) + 1) * 4;
        damageMobj(*actor.target, &actor, &actor, damage);
    }
}

void headAttack(Mobj& actor)
{
    if (!actor.target)
        return;

    faceTarget(actor);
    if (checkMeleeRange(actor))
    {
        int damage = (randomness().forPlay() % 6 + 1) * 10;
        damageMobj(*actor.target, &actor, &actor, damage);
        return;
    }

    // launch a missile
    spawnMissile(actor, actor.target, MobjType::Headshot);
}

void cyberAttack(Mobj& actor)
{
    if (!actor.target)
        return;

    faceTarget(actor);
    spawnMissile(actor, actor.target, MobjType::Rocket);
}

void bruisAttack(Mobj& actor)
{
    if (!actor.target)
        return;

    if (checkMeleeRange(actor))
    {
        startSound(&actor, SfxEnum::Claw);
        int damage = (randomness().forPlay() % 8 + 1) * 10;
        damageMobj(*actor.target, &actor, &actor, damage);
        return;
    }

    // launch a missile
    spawnMissile(actor, actor.target, MobjType::Bruisershot);
}

//
// skelMissile
//
void skelMissile(Mobj& actor)
{
    if (!actor.target)
        return;

    faceTarget(actor);
    actor.z += 16 * FRACUNIT; // so missile spawns higher
    Mobj* mo = spawnMissile(actor, actor.target, MobjType::Tracer);
    actor.z -= 16 * FRACUNIT; // back to normal

    mo->x += mo->momx;
    mo->y += mo->momy;
    mo->tracer = actor.target;
}

void tracer(Mobj& actor)
{
    // As in spawnMissile: dist is a tic count once divided by the missile's raw
    // speed, and is then the plain integer divisor of the height difference.

    if (gameClock().gametic & 3)
        return;

    // spawn a puff of smoke behind the rocket
    spawnPuff(actor.x, actor.y, actor.z);

    Mobj* th = spawnMobj(
        actor.x - actor.momx, actor.y - actor.momy, actor.z, MobjType::Smoke);

    th->momz = FRACUNIT;
    th->tics -= randomness().forPlay() & 3;
    if (th->tics < 1)
        th->tics = 1;

    // adjust direction
    Mobj* dest = actor.tracer;

    if (!dest || dest->health <= 0)
        return;

    // change angle
    angle_t exact = pointToAngle2(actor.x, actor.y, dest->x, dest->y);

    if (exact != actor.angle)
    {
        if (exact - actor.angle > ang180)
        {
            actor.angle -= TRACEANGLE;
            if (exact - actor.angle < ang180)
                actor.angle = exact;
        }
        else
        {
            actor.angle += TRACEANGLE;
            if (exact - actor.angle > ang180)
                actor.angle = exact;
        }
    }

    const auto fine = actor.angle.fineIndex();
    actor.momx = FixedMul(Fixed {actor.info->speed}, finecosine[fine]);
    actor.momy = FixedMul(Fixed {actor.info->speed}, finesine[fine]);

    // change slope
    int dist = approxDistance(dest->x - actor.x, dest->y - actor.y).raw;

    dist = dist / actor.info->speed;

    if (dist < 1)
        dist = 1;
    fixed_t slope = (dest->z + 40 * FRACUNIT - actor.z) / dist;

    if (slope < actor.momz)
        actor.momz -= FRACUNIT / 8;
    else
        actor.momz += FRACUNIT / 8;
}

void skelWhoosh(Mobj& actor)
{
    if (!actor.target)
        return;

    faceTarget(actor);
    startSound(&actor, SfxEnum::Skeswg);
}

void skelFist(Mobj& actor)
{
    if (!actor.target)
        return;

    faceTarget(actor);

    if (checkMeleeRange(actor))
    {
        int damage = ((randomness().forPlay() % 10) + 1) * 6;
        startSound(&actor, SfxEnum::Skepch);
        damageMobj(*actor.target, &actor, &actor, damage);
    }
}

//
// vileCheck
// Detect a corpse that could be raised.
//
bool vileCheck(Mobj* thing)
{
    auto& ai = enemyAI();

    if (!(hasFlag(thing->flags, MobjFlag::Corpse)))
        return true; // not a monster

    if (thing->tics != -1)
        return true; // not lying still yet

    if (thing->info->raisestate == StateNum::Null)
        return true; // monster doesn't have a raise state

    fixed_t maxdist = thing->info->radius + mobjinfo[toIndex(MobjType::Vile)].radius;

    if (doom_abs(thing->x - ai.viletryx) > maxdist
        || doom_abs(thing->y - ai.viletryy) > maxdist)
        return true; // not actually touching

    ai.corpsehit = thing;
    ai.corpsehit->momx = ai.corpsehit->momy = fixed_t {};
    ai.corpsehit->height <<= 2;
    bool check = checkPosition(*ai.corpsehit, ai.corpsehit->x, ai.corpsehit->y);
    ai.corpsehit->height >>= 2;

    if (!check)
        return true; // doesn't fit here

    return false; // got one, so stop checking
}

//
// vileChase
// Check for ressurecting a body
//
void vileChase(Mobj& actor)
{
    auto& ai = enemyAI();

    if (actor.movedir != toIndex(DirType::NoDir))
    {
        // check for corpses to raise
        ai.viletryx = actor.x + actor.info->speed * xspeed[actor.movedir];
        ai.viletryy = actor.y + actor.info->speed * yspeed[actor.movedir];

        int xl = (ai.viletryx - bmaporgx - MAXRADIUS * 2).raw >> MAPBLOCKSHIFT;
        int xh = (ai.viletryx - bmaporgx + MAXRADIUS * 2).raw >> MAPBLOCKSHIFT;
        int yl = (ai.viletryy - bmaporgy - MAXRADIUS * 2).raw >> MAPBLOCKSHIFT;
        int yh = (ai.viletryy - bmaporgy + MAXRADIUS * 2).raw >> MAPBLOCKSHIFT;

        for (int bx = xl; bx <= xh; bx++)
        {
            for (int by = yl; by <= yh; by++)
            {
                // Call vileCheck to check
                // whether object is a corpse
                // that canbe raised.
                if (!forEachThingInBlock(bx, by, vileCheck))
                {
                    // got one!
                    Mobj* temp = actor.target;
                    actor.target = ai.corpsehit;
                    faceTarget(actor);
                    actor.target = temp;

                    setMobjState(actor, StateNum::VileHeal1);
                    startSound(ai.corpsehit, SfxEnum::Slop);
                    MobjInfo* info = ai.corpsehit->info;

                    setMobjState(*ai.corpsehit, info->raisestate);
                    ai.corpsehit->height <<= 2;
                    ai.corpsehit->flags = info->flags;
                    ai.corpsehit->health = info->spawnhealth;
                    ai.corpsehit->target = nullptr;

                    return;
                }
            }
        }
    }

    // Return to normal attack.
    chase(actor);
}

//
// vileStart
//
void vileStart(Mobj& actor)
{
    startSound(&actor, SfxEnum::Vilatk);
}

//
// fire
// Keep fire in front of player unless out of sight
//
void startFire(Mobj& actor)
{
    startSound(&actor, SfxEnum::Flamst);
    fire(actor);
}

void fireCrackle(Mobj& actor)
{
    startSound(&actor, SfxEnum::Flame);
    fire(actor);
}

void fire(Mobj& actor)
{
    Mobj* dest = actor.tracer;
    if (!dest)
        return;

    // don't move it if the vile lost sight
    if (!checkSight(actor.target, dest))
        return;

    const auto anFine = dest->angle.fineIndex();

    unsetThingPosition(actor);
    actor.x = dest->x + FixedMul(24 * FRACUNIT, finecosine[anFine]);
    actor.y = dest->y + FixedMul(24 * FRACUNIT, finesine[anFine]);
    actor.z = dest->z;
    setThingPosition(actor);
}

//
// vileTarget
// Spawn the hellfire
//
void vileTarget(Mobj& actor)
{
    if (!actor.target)
        return;

    faceTarget(actor);

    Mobj* fog =
        spawnMobj(actor.target->x, actor.target->x, actor.target->z, MobjType::Fire);

    actor.tracer = fog;
    fog->target = &actor;
    fog->tracer = actor.target;
    fire(*fog);
}

//
// vileAttack
//
void vileAttack(Mobj& actor)
{
    if (!actor.target)
        return;

    faceTarget(actor);

    if (!checkSight(&actor, actor.target))
        return;

    startSound(&actor, SfxEnum::Barexp);
    damageMobj(*actor.target, &actor, &actor, 20);
    actor.target->momz = 1000 * FRACUNIT / actor.target->info->mass;

    const auto anFine = actor.angle.fineIndex();

    Mobj* fire = actor.tracer;

    if (!fire)
        return;

    // move the fire between the vile and the player
    fire->x = actor.target->x - FixedMul(24 * FRACUNIT, finecosine[anFine]);
    fire->y = actor.target->y - FixedMul(24 * FRACUNIT, finesine[anFine]);
    radiusAttack(*fire, &actor, 70);
}

//
// Mancubus attack,
// firing three missiles (bruisers)
// in three different directions?
// Doesn't look like it.
//
void fatRaise(Mobj& actor)
{
    faceTarget(actor);
    startSound(&actor, SfxEnum::Manatk);
}

void fatAttack1(Mobj& actor)
{
    faceTarget(actor);
    // Change direction  to ...
    actor.angle += FATSPREAD;
    spawnMissile(actor, actor.target, MobjType::Fatshot);

    Mobj* mo = spawnMissile(actor, actor.target, MobjType::Fatshot);
    mo->angle += FATSPREAD;
    const auto an1Fine = mo->angle.fineIndex();
    mo->momx = FixedMul(Fixed {mo->info->speed}, finecosine[an1Fine]);
    mo->momy = FixedMul(Fixed {mo->info->speed}, finesine[an1Fine]);
}

void fatAttack2(Mobj& actor)
{
    faceTarget(actor);
    // Now here choose opposite deviation.
    actor.angle -= FATSPREAD;
    spawnMissile(actor, actor.target, MobjType::Fatshot);

    Mobj* mo = spawnMissile(actor, actor.target, MobjType::Fatshot);
    mo->angle -= FATSPREAD * 2;
    const auto an2Fine = mo->angle.fineIndex();
    mo->momx = FixedMul(Fixed {mo->info->speed}, finecosine[an2Fine]);
    mo->momy = FixedMul(Fixed {mo->info->speed}, finesine[an2Fine]);
}

void fatAttack3(Mobj& actor)
{
    faceTarget(actor);

    Mobj* mo = spawnMissile(actor, actor.target, MobjType::Fatshot);
    mo->angle -= FATSPREAD / 2;
    const auto an3Fine = mo->angle.fineIndex();
    mo->momx = FixedMul(Fixed {mo->info->speed}, finecosine[an3Fine]);
    mo->momy = FixedMul(Fixed {mo->info->speed}, finesine[an3Fine]);

    mo = spawnMissile(actor, actor.target, MobjType::Fatshot);
    mo->angle += FATSPREAD / 2;
    const auto an4Fine = mo->angle.fineIndex();
    mo->momx = FixedMul(Fixed {mo->info->speed}, finecosine[an4Fine]);
    mo->momy = FixedMul(Fixed {mo->info->speed}, finesine[an4Fine]);
}

//
// SkullAttack
// Fly at the player like a missile.
//
void skullAttack(Mobj& actor)
{
    if (!actor.target)
        return;

    Mobj* dest = actor.target;
    actor.flags = withFlags(actor.flags, MobjFlag::SkullFly);

    startSound(&actor, actor.info->attacksound);
    faceTarget(actor);
    const auto fine = actor.angle.fineIndex();
    actor.momx = FixedMul(SKULLSPEED, finecosine[fine]);
    actor.momy = FixedMul(SKULLSPEED, finesine[fine]);
    int dist = approxDistance(dest->x - actor.x, dest->y - actor.y).raw;
    dist = dist / SKULLSPEED.raw;

    if (dist < 1)
        dist = 1;
    actor.momz = (dest->z + (dest->height >> 1) - actor.z) / dist;
}

//
// painShootSkull
// Spawn a lost soul and launch it at the target
//
void painShootSkull(Mobj& actor, angle_t angle)
{
    auto& thinkers = thinkerList();

    // count total number of skull currently on the level
    int count = 0;

    Thinker* currentthinker = thinkers.cap.next;
    while (currentthinker != &thinkers.cap)
    {
        if (currentthinker->kind() == ThinkerKind::Mobj && !currentthinker->removed
            && (reinterpret_cast<Mobj*>(currentthinker))->type == MobjType::Skull)
            count++;
        currentthinker = currentthinker->next;
    }

    // if there are allready 20 skulls on the level,
    // don't spit another one
    if (count > 20)
        return;

    // okay, there's playe for another one
    const auto anFine = angle.fineIndex();

    fixed_t prestep =
        4 * FRACUNIT
        + 3 * (actor.info->radius + mobjinfo[toIndex(MobjType::Skull)].radius) / 2;

    fixed_t x = actor.x + FixedMul(prestep, finecosine[anFine]);
    fixed_t y = actor.y + FixedMul(prestep, finesine[anFine]);
    fixed_t z = actor.z + 8 * FRACUNIT;

    Mobj* newmobj = spawnMobj(x, y, z, MobjType::Skull);

    // Check for movements.
    if (!tryMove(*newmobj, newmobj->x, newmobj->y))
    {
        // kill it immediately
        damageMobj(*newmobj, &actor, &actor, 10000);
        return;
    }

    newmobj->target = actor.target;
    skullAttack(*newmobj);
}

//
// painAttack
// Spawn a lost soul and launch it at the target
//
void painAttack(Mobj& actor)
{
    if (!actor.target)
        return;

    faceTarget(actor);
    painShootSkull(actor, actor.angle);
}

void painDie(Mobj& actor)
{
    fall(actor);
    painShootSkull(actor, actor.angle + ang90);
    painShootSkull(actor, actor.angle + ang180);
    painShootSkull(actor, actor.angle + ang270);
}

void scream(Mobj& actor)
{
    SfxEnum sound;

    switch (actor.info->deathsound)
    {
        case SfxEnum::None:
            return;

        case SfxEnum::Podth1:
        case SfxEnum::Podth2:
        case SfxEnum::Podth3:
            sound = static_cast<SfxEnum>(toIndex(SfxEnum::Podth1)
                                         + randomness().forPlay() % 3);
            break;

        case SfxEnum::Bgdth1:
        case SfxEnum::Bgdth2:
            sound = static_cast<SfxEnum>(toIndex(SfxEnum::Bgdth1)
                                         + randomness().forPlay() % 2);
            break;

        default:
            sound = actor.info->deathsound;
            break;
    }

    // Check for bosses.
    if (actor.type == MobjType::Spider || actor.type == MobjType::Cyborg)
    {
        // full volume
        startSound(0, sound);
    }
    else
        startSound(&actor, sound);
}

void xScream(Mobj& actor)
{
    startSound(&actor, SfxEnum::Slop);
}

void pain(Mobj& actor)
{
    if (actor.info->painsound != SfxEnum::None)
        startSound(&actor, actor.info->painsound);
}

void fall(Mobj& actor)
{
    // actor is on ground, it can be walked over
    actor.flags = withoutFlags(actor.flags, MobjFlag::Solid);

    // So change this if corpse objects
    // are meant to be obstacles.
}

//
// explode
//
void explode(Mobj& thingy)
{
    radiusAttack(thingy, thingy.target, 128);
}

//
// bossDeath
// Possibly trigger special effects
// if on first boss level
//
void bossDeath(Mobj& mo)
{
    Line junk;

    auto& thinkers = thinkerList();
    const auto& version = gameVersion();
    const auto& session = gameSession();
    const auto& players_ = playerState();

    if (version.gamemode == GameMode::Commercial)
    {
        if (session.gamemap != 7)
            return;

        if ((mo.type != MobjType::Fatso) && (mo.type != MobjType::Baby))
            return;
    }
    else
    {
        switch (session.gameepisode)
        {
            case 1:
                if (session.gamemap != 8)
                    return;

                if (mo.type != MobjType::Bruiser)
                    return;
                break;

            case 2:
                if (session.gamemap != 8)
                    return;

                if (mo.type != MobjType::Cyborg)
                    return;
                break;

            case 3:
                if (session.gamemap != 8)
                    return;

                if (mo.type != MobjType::Spider)
                    return;

                break;

            case 4:
                switch (session.gamemap)
                {
                    case 6:
                        if (mo.type != MobjType::Cyborg)
                            return;
                        break;

                    case 8:
                        if (mo.type != MobjType::Spider)
                            return;
                        break;

                    default:
                        return;
                        break;
                }
                break;

            default:
                if (session.gamemap != 8)
                    return;
                break;
        }
    }

    // make sure there is a player alive for victory
    const auto anyPlayerAlive = [&players_]
    {
        for (int i = 0; i < MAXPLAYERS; i++)
            if (players_.playeringame[i] && players_.players[i].health > 0)
                return true;

        return false;
    };

    if (!anyPlayerAlive())
        return; // no one left alive, so do not end game

    // scan the remaining thinkers to see
    // if all bosses are dead
    for (Thinker* th = thinkers.cap.next; th != &thinkers.cap; th = th->next)
    {
        if (th->kind() != ThinkerKind::Mobj || th->removed)
            continue;

        Mobj* mo2 = reinterpret_cast<Mobj*>(th);
        if (mo2 != &mo && mo2->type == mo.type && mo2->health > 0)
        {
            // other boss not dead
            return;
        }
    }

    // victory!
    if (version.gamemode == GameMode::Commercial)
    {
        if (session.gamemap == 7)
        {
            if (mo.type == MobjType::Fatso)
            {
                junk.tag = 666;
                doFloor(junk, FloorType::LowerFloorToLowest);
                return;
            }

            if (mo.type == MobjType::Baby)
            {
                junk.tag = 667;
                doFloor(junk, FloorType::RaiseToTexture);
                return;
            }
        }
    }
    else
    {
        switch (session.gameepisode)
        {
            case 1:
                junk.tag = 666;
                doFloor(junk, FloorType::LowerFloorToLowest);
                return;
                break;

            case 4:
                switch (session.gamemap)
                {
                    case 6:
                        junk.tag = 666;
                        doDoor(junk, DoorType::BlazeOpen);
                        return;
                        break;

                    case 8:
                        junk.tag = 666;
                        doFloor(junk, FloorType::LowerFloorToLowest);
                        return;
                        break;
                }
        }
    }

    exitLevel();
}

void hoof(Mobj& mo)
{
    startSound(&mo, SfxEnum::Hoof);
    chase(mo);
}

void metal(Mobj& mo)
{
    startSound(&mo, SfxEnum::Metal);
    chase(mo);
}

void babyMetal(Mobj& mo)
{
    startSound(&mo, SfxEnum::Bspwlk);
    chase(mo);
}

void openShotgun2(Player& player, PspDef&)
{
    startSound(player.mo, SfxEnum::Dbopn);
}

void loadShotgun2(Player& player, PspDef&)
{
    startSound(player.mo, SfxEnum::Dbload);
}

void closeShotgun2(Player& player, PspDef& psp)
{
    startSound(player.mo, SfxEnum::Dbcls);
    reFire(player, psp);
}

void brainAwake(Mobj&)
{
    auto& thinkers = thinkerList();
    auto& ai = enemyAI();

    // find all the target spots
    ai.braintargets.clear();
    ai.braintargeton = 0;

    Thinker* thinker = thinkers.cap.next;
    for (thinker = thinkers.cap.next; thinker != &thinkers.cap;
         thinker = thinker->next)
    {
        if (thinker->kind() != ThinkerKind::Mobj || thinker->removed)
            continue; // not a mobj

        Mobj* m = reinterpret_cast<Mobj*>(thinker);

        if (m->type == MobjType::Bosstarget)
        {
            ai.braintargets.add(m);
        }
    }

    startSound(0, SfxEnum::Bossit);
}

void brainPain(Mobj&)
{
    startSound(0, SfxEnum::Bospn);
}

void brainScream(Mobj& mo)
{
    for (fixed_t x = mo.x - 196 * FRACUNIT; x < mo.x + 320 * FRACUNIT;
         x += FRACUNIT * 8)
    {
        fixed_t y = mo.y - 320 * FRACUNIT;
        // vanilla's raw 128 added to a whole-unit-scaled random; kept as it stands.
        fixed_t z = Fixed {128} + randomness().forPlay() * 2 * FRACUNIT;
        Mobj* th = spawnMobj(x, y, z, MobjType::Rocket);
        th->momz = Fixed {randomness().forPlay() * 512};

        setMobjState(*th, StateNum::Brainexplode1);

        th->tics -= randomness().forPlay() & 7;
        if (th->tics < 1)
            th->tics = 1;
    }

    startSound(0, SfxEnum::Bosdth);
}

void brainExplode(Mobj& mo)
{
    fixed_t x =
        mo.x + Fixed {(randomness().forPlay() - randomness().forPlay()) * 2048};
    fixed_t y = mo.y;
    fixed_t z = Fixed {128} + randomness().forPlay() * 2 * FRACUNIT;
    Mobj* th = spawnMobj(x, y, z, MobjType::Rocket);
    th->momz = Fixed {randomness().forPlay() * 512};

    setMobjState(*th, StateNum::Brainexplode1);

    th->tics -= randomness().forPlay() & 7;
    if (th->tics < 1)
        th->tics = 1;
}

void brainDie(Mobj&)
{
    exitLevel();
}

void brainSpit(Mobj& mo)
{
    auto& ai = enemyAI();

    ai.brainSpitEasy ^= 1;
    if (gameSession().gameskill <= Skill::Easy && (!ai.brainSpitEasy))
        return;

    // shoot a cube at current target
    Mobj* targ = ai.braintargets[ai.braintargeton];
    ai.braintargeton = (ai.braintargeton + 1) % ai.braintargets.size();

    // spawn brain missile
    Mobj* newmobj = spawnMissile(mo, targ, MobjType::Spawnshot);
    newmobj->target = targ;
    // Vanilla divides the raw values as plain integers here - the result is a tic
    // count, not a length. A fixed-point divide would scale it by 65536.
    newmobj->reactiontime =
        ((targ->y - mo.y).raw / newmobj->momy.raw) / newmobj->state->tics;

    startSound(0, SfxEnum::Bospit);
}

// travelling cube sound
void spawnSound(Mobj& mo)
{
    startSound(&mo, SfxEnum::Boscub);
    spawnFly(mo);
}

void spawnFly(Mobj& mo)
{
    MobjType type;

    if (--mo.reactiontime)
        return; // still flying

    Mobj* targ = mo.target;

    // First spawn teleport fog.
    Mobj* fog = spawnMobj(targ->x, targ->y, targ->z, MobjType::Spawnfire);
    startSound(fog, SfxEnum::Telept);

    // Randomly select monster to spawn.
    int r = randomness().forPlay();

    // Probability distribution (kind of :),
    // decreasing likelihood.
    if (r < 50)
        type = MobjType::Troop;
    else if (r < 90)
        type = MobjType::Sergeant;
    else if (r < 120)
        type = MobjType::Shadows;
    else if (r < 130)
        type = MobjType::Pain;
    else if (r < 160)
        type = MobjType::Head;
    else if (r < 162)
        type = MobjType::Vile;
    else if (r < 172)
        type = MobjType::Undead;
    else if (r < 192)
        type = MobjType::Baby;
    else if (r < 222)
        type = MobjType::Fatso;
    else if (r < 246)
        type = MobjType::Knight;
    else
        type = MobjType::Bruiser;

    Mobj* newmobj = spawnMobj(targ->x, targ->y, targ->z, type);
    if (lookForPlayers(*newmobj, true))
        setMobjState(*newmobj, static_cast<StateNum>(newmobj->info->seestate));

    // telefrag anything in this spot
    teleportMove(*newmobj, newmobj->x, newmobj->y);

    // remove self (i.e., cube).
    removeMobj(mo);
}

void playerScream(Mobj& mo)
{
    // Default death sound.
    SfxEnum sound = SfxEnum::Pldeth;

    if ((gameVersion().gamemode == GameMode::Commercial) && (mo.health < -50))
    {
        // IF THE PLAYER DIES
        // LESS THAN -50% WITHOUT GIBBING
        sound = SfxEnum::Pdiehi;
    }

    startSound(&mo, sound);
}
} // namespace Doom
