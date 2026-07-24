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
// Rewritten into namespace Doom out of vanilla p_enemy. The A_* codepointers are
// Mobj methods now (chase, look, the attacks, the deaths) and, for the super-
// shotgun, Player methods; Sim/Info.cpp's state table installs each as &Mobj::name /
// &Player::name. The internal AI they call is Mobj methods too (move, tryWalk,
// newChaseDir, lookForPlayers, the range checks, painShootSkull). What stays a free
// function is what has no single owning mobj: the recursiveSound flood over sectors,
// noiseAlert (a source and an emitter), and the vileCheck blockmap callback. The AI
// scratch is file-local; soundtarget stays global (p_saveg archives it).
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
constexpr Angle FATSPREAD = ang90 / 8;
constexpr Fixed SKULLSPEED = 20 * FRACUNIT;

// P_NewChaseDir movement LUTs and the transient targets the AI threads through its
// state actions (the vile's corpse, the fat/brain spit targets). All file-local;
// soundtarget alone is shared (now a SoundTarget Engine member, reached above).
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
Array<Fixed, 8> xspeed = {FRACUNIT,
                          Fixed {47000},
                          Fixed {},
                          Fixed {-47000},
                          -FRACUNIT,
                          Fixed {-47000},
                          Fixed {},
                          Fixed {47000}};
Array<Fixed, 8> yspeed = {Fixed {},
                          Fixed {47000},
                          FRACUNIT,
                          Fixed {47000},
                          Fixed {},
                          Fixed {-47000},
                          -FRACUNIT,
                          Fixed {-47000}};
Angle TRACEANGLE {0xc000000};
// The monster-AI scratch now lives on the Engine (Sim/EnemyAI.h, moved by the file-scope-statics
// sweep - REFACTOR.md, Step 5). vileCheck, vileChase, brainAwake and brainSpit each hoist
// enemyAI() once and reach its members through it, rather than through file-scope reference
// aliases (REFACTOR.md, Step 9 strand (a)). (The const direction/speed tables above stay
// file-local.)

// Forward declarations so the file's own call order needs no rearranging.
void recursiveSound(Sector* sec, int soundblocks);
void noiseAlert(Mobj& target, Mobj& emmiter);
bool vileCheck(Mobj* thing);

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

        if (!clipping().openrange.isPositive())
            continue; // closed door

        if (level().sides[check->sidenum[0]].sector == sec)
            other = level().sides[check->sidenum[1]].sector;
        else
            other = level().sides[check->sidenum[0]].sector;

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
bool Mobj::checkMeleeRange()
{
    if (!target)
        return false;

    Mobj* pl = target;
    Fixed dist = approxDistance(pl->x - x, pl->y - y);

    if (dist >= MELEERANGE - 20 * FRACUNIT + pl->info->radius)
        return false;

    if (!checkSight(this, target))
        return false;

    return true;
}

//
// checkMissileRange
//
bool Mobj::checkMissileRange()
{
    // Vanilla declares dist Fixed and then shifts it down to whole units
    // half way through, after which every use of it is a plain integer -
    // compared against 14*64, 196, 200, 160 and P_Random's 0..255.

    if (!checkSight(this, target))
        return false;

    if (hasFlag(flags, MobjFlag::JustHit))
    {
        // the target just hit the enemy,
        // so fight back!
        flags = withoutFlags(flags, MobjFlag::JustHit);
        return true;
    }

    if (reactiontime)
        return false; // do not attack yet

    // OPTIMIZE: get this from a global checksight
    Fixed distance = approxDistance(x - target->x, y - target->y) - 64 * FRACUNIT;

    if (info->meleestate == StateNum::Null)
        distance -= 128 * FRACUNIT; // no melee attack, so fire more

    int dist = distance.toInt();

    if (type == MobjType::Vile)
    {
        if (dist > 14 * 64)
            return false; // too far away
    }

    if (type == MobjType::Undead)
    {
        if (dist < 196)
            return false; // close for fist attack
        dist >>= 1;
    }

    if (type == MobjType::Cyborg || type == MobjType::Spider
        || type == MobjType::Skull)
    {
        dist >>= 1;
    }

    if (dist > 200)
        dist = 200;

    if (type == MobjType::Cyborg && dist > 160)
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
bool Mobj::move()
{
    // warning: 'catch', 'throw', and 'try'
    // are all C++ reserved words

    auto& c = clipping();

    if (movedir == toIndex(DirType::NoDir))
        return false;

    if (static_cast<unsigned>(movedir) >= 8)
        fatalError("Error: Weird *this->movedir!");

    Fixed tryx = x + info->speed * xspeed[movedir];
    Fixed tryy = y + info->speed * yspeed[movedir];

    bool try_ok = tryMove(tryx, tryy);

    if (!try_ok)
    {
        // open any specials
        if (hasFlag(flags, MobjFlag::Float) && c.floatok)
        {
            // must adjust height
            if (z < c.tmfloorz)
                z += FLOATSPEED;
            else
                z -= FLOATSPEED;

            flags = withFlags(flags, MobjFlag::InFloat);
            return true;
        }

        if (!c.numspechit)
            return false;

        movedir = toIndex(DirType::NoDir);
        bool good = false;
        while (c.numspechit--)
        {
            Line* ld = c.spechit[c.numspechit];
            // if the special is not a door
            // that can be opened,
            // return false
            if (ld->useSpecialLine(*this, 0))
                good = true;
        }
        return good;
    }
    else
    {
        flags = withoutFlags(flags, MobjFlag::InFloat);
    }

    if (!(hasFlag(flags, MobjFlag::Float)))
        z = floorz;

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
bool Mobj::tryWalk()
{
    if (!move())
    {
        return false;
    }

    movecount = randomness().forPlay() & 15;
    return true;
}

void Mobj::newChaseDir()
{
    Array<DirType, 3> d;

    int tdir;

    if (!target)
        fatalError("Error: newChaseDir: called with no target");

    DirType olddir = static_cast<DirType>(movedir);
    DirType turnaround = opposite[toIndex(olddir)];

    Fixed deltax = target->x - x;
    Fixed deltay = target->y - y;

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
        movedir = toIndex(diags[(deltay.isNegative() << 1) + deltax.isPositive()]);
        if (movedir != toIndex(turnaround) && tryWalk())
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
        movedir = toIndex(d[1]);
        if (tryWalk())
        {
            // either moved forward or attacked
            return;
        }
    }

    if (d[2] != DirType::NoDir)
    {
        movedir = toIndex(d[2]);

        if (tryWalk())
            return;
    }

    // there is no direct path to the player,
    // so pick another direction.
    if (olddir != DirType::NoDir)
    {
        movedir = toIndex(olddir);

        if (tryWalk())
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
                movedir = tdir;

                if (tryWalk())
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
                movedir = tdir;

                if (tryWalk())
                    return;
            }
        }
    }

    if (turnaround != DirType::NoDir)
    {
        movedir = toIndex(turnaround);
        if (tryWalk())
            return;
    }

    movedir = toIndex(DirType::NoDir); // can not move
}

//
// lookForPlayers
// If allaround is false, only look 180 degrees in front.
// Returns true if a player is targeted.
//
bool Mobj::lookForPlayers(bool allaround)
{
    int c = 0;
    int stop = (lastlook - 1) & 3;

    auto& players_ = playerState();

    for (;; lastlook = (lastlook + 1) & 3)
    {
        if (!players_.playeringame[lastlook])
            continue;

        if (c++ == 2 || lastlook == stop)
        {
            // done looking
            return false;
        }

        Player* playerToUse = &players_.players[lastlook];

        if (playerToUse->health <= 0)
            continue; // dead

        if (!checkSight(this, playerToUse->mo))
            continue; // out of sight

        if (!allaround)
        {
            Angle an =
                pointToAngle2(x, y, playerToUse->mo->x, playerToUse->mo->y) - angle;

            if (an > ang90 && an < ang270)
            {
                Fixed dist =
                    approxDistance(playerToUse->mo->x - x, playerToUse->mo->y - y);
                // if real close, react anyway
                if (dist > MELEERANGE)
                    continue; // behind back
            }
        }

        target = playerToUse->mo;
        return true;
    }

    return false;
}

//
// keenDie
// DOOM II special, map 32.
// Uses special tag 666.
//
void Mobj::keenDie()
{
    Line junk;

    auto& thinkers = thinkerList();

    fall();

    // scan the remaining thinkers
    // to see if all Keens are dead
    for (Thinker* th = thinkers.cap.next; th != &thinkers.cap; th = th->next)
    {
        if (th->kind() != ThinkerKind::Mobj || th->removed)
            continue;

        Mobj* mo2 = reinterpret_cast<Mobj*>(th);
        if (mo2 != this && mo2->type == type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    junk.tag = 666;
    junk.doDoor(DoorType::Open);
}

//
// ACTION ROUTINES
//

//
// look
// Stay in state until a player is sighted.
//
void Mobj::look()
{
    threshold = 0; // any shot will wake up
    Mobj* targ = subsector->sector->soundtarget;

    // go into chase state
    auto seeyou = [&]
    {
        if (info->seesound != SfxEnum::None)
        {
            SfxEnum sound;

            switch (info->seesound)
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
                    sound = info->seesound;
                    break;
            }

            if (type == MobjType::Spider || type == MobjType::Cyborg)
            {
                // full volume
                startSound(0, sound);
            }
            else
                startSound(this, sound);
        }

        setState(static_cast<StateNum>(info->seestate));
    };

    if (targ && (hasFlag(targ->flags, MobjFlag::Shootable)))
    {
        target = targ;

        // An ambush monster waits for line of sight; any other wakes on the
        // sound target alone.
        if (!hasFlag(flags, MobjFlag::Ambush) || checkSight(this, target))
        {
            seeyou();
            return;
        }
    }

    if (!lookForPlayers(false))
        return;

    seeyou();
}

//
// chase
// Actor has a melee attack,
// so it tries to close as fast as possible
//
void Mobj::chase()
{
    // SIGNED on purpose: vanilla declares this int, so the two tests below are a
    // signed comparison of an angle difference. As an unsigned raw angle, `> 0`
    // would be true for every non-zero value and the `< 0` branch unreachable -
    // which desyncs demo1 at tic 89, monsters turning the wrong way.

    if (reactiontime)
        reactiontime--;

    // modify target threshold
    if (threshold)
    {
        if (!target || target->health <= 0)
        {
            threshold = 0;
        }
        else
            threshold--;
    }

    // turn towards movement direction if not there yet
    if (movedir < 8)
    {
        angle = Angle {angle.raw & (7u << 29)};
        int delta = (int) (angle - Angle {(unsigned) movedir << 29}).raw;

        if (delta > 0)
            angle -= ang90 / 2u;
        else if (delta < 0)
            angle += ang90 / 2u;
    }

    if (!target || !(hasFlag(target->flags, MobjFlag::Shootable)))
    {
        // look for a new target
        if (lookForPlayers(true))
            return; // got a new target

        setState(static_cast<StateNum>(info->spawnstate));
        return;
    }

    const auto& session = gameSession();
    const auto& opts = launchOptions();

    // do not attack twice in a row
    if (hasFlag(flags, MobjFlag::JustAttacked))
    {
        flags = withoutFlags(flags, MobjFlag::JustAttacked);
        if (session.gameskill != Skill::Nightmare && !opts.fastparm)
            newChaseDir();
        return;
    }

    // check for melee attack
    if (info->meleestate != StateNum::Null && checkMeleeRange())
    {
        if (info->attacksound != SfxEnum::None)
            startSound(this, info->attacksound);

        setState(info->meleestate);
        return;
    }

    // check for missile attack
    if (info->missilestate != StateNum::Null)
    {
        // Below Nightmare/fast, a monster that still has moves left in its
        // current direction holds fire; otherwise it fires if in range. Either
        // way a "no" falls through to the chase logic below. checkMissileRange is
        // evaluated only when not holding fire, exactly as the two gotos did.
        const bool holdFire =
            session.gameskill < Skill::Nightmare && !opts.fastparm && movecount;

        if (!holdFire && checkMissileRange())
        {
            setState(info->missilestate);
            flags = withFlags(flags, MobjFlag::JustAttacked);
            return;
        }
    }

    // possibly choose another target
    if (session.netgame && !threshold && !checkSight(this, target))
    {
        if (lookForPlayers(true))
            return; // got a new target
    }

    // chase towards player
    if (--movecount < 0 || !move())
    {
        newChaseDir();
    }

    // make active sound
    if (info->activesound != SfxEnum::None && randomness().forPlay() < 3)
    {
        startSound(this, info->activesound);
    }
}

//
// faceTarget
//
void Mobj::faceTarget()
{
    if (!target)
        return;

    flags = withoutFlags(flags, MobjFlag::Ambush);

    angle = pointToAngle2(x, y, target->x, target->y);

    if (hasFlag(target->flags, MobjFlag::Shadow))
        angle += Angle {(unsigned) (randomness().forPlay() - randomness().forPlay())
                        << 21};
}

//
// posAttack
//
void Mobj::posAttack()
{
    if (!target)
        return;

    faceTarget();
    Angle angleToUse = angle;
    Fixed slope = aimLineAttack(this, angleToUse, MISSILERANGE).slope;

    startSound(this, SfxEnum::Pistol);
    angleToUse +=
        Angle {(unsigned) (randomness().forPlay() - randomness().forPlay()) << 20};
    int damage = ((randomness().forPlay() % 5) + 1) * 3;
    lineAttack(angleToUse, MISSILERANGE, slope, damage);
}

void Mobj::sPosAttack()
{
    Angle angleToUse {};

    if (!target)
        return;

    startSound(this, SfxEnum::Shotgn);
    faceTarget();
    Angle bangle = angle;
    Fixed slope = aimLineAttack(this, bangle, MISSILERANGE).slope;

    for (int i = 0; i < 3; i++)
    {
        angleToUse =
            bangle
            + Angle {(unsigned) (randomness().forPlay() - randomness().forPlay())
                     << 20};
        int damage = ((randomness().forPlay() % 5) + 1) * 3;
        lineAttack(angleToUse, MISSILERANGE, slope, damage);
    }
}

void Mobj::cPosAttack()
{
    Angle angleToUse {};

    if (!target)
        return;

    startSound(this, SfxEnum::Shotgn);
    faceTarget();
    Angle bangle = angle;
    Fixed slope = aimLineAttack(this, bangle, MISSILERANGE).slope;

    angleToUse =
        bangle
        + Angle {(unsigned) (randomness().forPlay() - randomness().forPlay()) << 20};
    int damage = ((randomness().forPlay() % 5) + 1) * 3;
    lineAttack(angleToUse, MISSILERANGE, slope, damage);
}

void Mobj::cPosRefire()
{
    // keep firing unless target got out of sight
    faceTarget();

    if (randomness().forPlay() < 40)
        return;

    if (!target || target->health <= 0 || !checkSight(this, target))
    {
        setState(static_cast<StateNum>(info->seestate));
    }
}

void Mobj::spidRefire()
{
    // keep firing unless target got out of sight
    faceTarget();

    if (randomness().forPlay() < 10)
        return;

    if (!target || target->health <= 0 || !checkSight(this, target))
    {
        setState(static_cast<StateNum>(info->seestate));
    }
}

void Mobj::bspiAttack()
{
    if (!target)
        return;

    faceTarget();

    // launch a missile
    spawnMissile(target, MobjType::Arachplaz);
}

//
// troopAttack
//
void Mobj::troopAttack()
{
    if (!target)
        return;

    faceTarget();
    if (checkMeleeRange())
    {
        startSound(this, SfxEnum::Claw);
        int damage = (randomness().forPlay() % 8 + 1) * 3;
        target->damage(this, this, damage);
        return;
    }

    // launch a missile
    spawnMissile(target, MobjType::Troopshot);
}

void Mobj::sargAttack()
{
    if (!target)
        return;

    faceTarget();
    if (checkMeleeRange())
    {
        int damage = ((randomness().forPlay() % 10) + 1) * 4;
        target->damage(this, this, damage);
    }
}

void Mobj::headAttack()
{
    if (!target)
        return;

    faceTarget();
    if (checkMeleeRange())
    {
        int damage = (randomness().forPlay() % 6 + 1) * 10;
        target->damage(this, this, damage);
        return;
    }

    // launch a missile
    spawnMissile(target, MobjType::Headshot);
}

void Mobj::cyberAttack()
{
    if (!target)
        return;

    faceTarget();
    spawnMissile(target, MobjType::Rocket);
}

void Mobj::bruisAttack()
{
    if (!target)
        return;

    if (checkMeleeRange())
    {
        startSound(this, SfxEnum::Claw);
        int damage = (randomness().forPlay() % 8 + 1) * 10;
        target->damage(this, this, damage);
        return;
    }

    // launch a missile
    spawnMissile(target, MobjType::Bruisershot);
}

//
// skelMissile
//
void Mobj::skelMissile()
{
    if (!target)
        return;

    faceTarget();
    z += 16 * FRACUNIT; // so missile spawns higher
    Mobj* mo = spawnMissile(target, MobjType::Tracer);
    z -= 16 * FRACUNIT; // back to normal

    mo->x += mo->momx;
    mo->y += mo->momy;
    mo->tracer = target;
}

void Mobj::traceTarget()
{
    // As in spawnMissile: dist is a tic count once divided by the missile's raw
    // speed, and is then the plain integer divisor of the height difference.

    if (gameClock().gametic & 3)
        return;

    // spawn a puff of smoke behind the rocket
    spawnPuff(x, y, z);

    Mobj* th = spawnMobj(x - momx, y - momy, z, MobjType::Smoke);

    th->momz = FRACUNIT;
    th->tics -= randomness().forPlay() & 3;
    if (th->tics < 1)
        th->tics = 1;

    // adjust direction
    Mobj* dest = tracer;

    if (!dest || dest->health <= 0)
        return;

    // change angle
    Angle exact = pointToAngle2(x, y, dest->x, dest->y);

    if (exact != angle)
    {
        if (exact - angle > ang180)
        {
            angle -= TRACEANGLE;
            if (exact - angle < ang180)
                angle = exact;
        }
        else
        {
            angle += TRACEANGLE;
            if (exact - angle > ang180)
                angle = exact;
        }
    }

    const auto fine = angle.fineIndex();
    momx = FixedMul(Fixed {info->speed}, finecosine()[fine]);
    momy = FixedMul(Fixed {info->speed}, finesine()[fine]);

    // change slope
    int dist = approxDistance(dest->x - x, dest->y - y).raw;

    dist = dist / info->speed;

    if (dist < 1)
        dist = 1;
    Fixed slope = (dest->z + 40 * FRACUNIT - z) / dist;

    if (slope < momz)
        momz -= FRACUNIT / 8;
    else
        momz += FRACUNIT / 8;
}

void Mobj::skelWhoosh()
{
    if (!target)
        return;

    faceTarget();
    startSound(this, SfxEnum::Skeswg);
}

void Mobj::skelFist()
{
    if (!target)
        return;

    faceTarget();

    if (checkMeleeRange())
    {
        int damage = ((randomness().forPlay() % 10) + 1) * 6;
        startSound(this, SfxEnum::Skepch);
        target->damage(this, this, damage);
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

    Fixed maxdist = thing->info->radius + mobjinfo()[toIndex(MobjType::Vile)].radius;

    if (doom_abs(thing->x - ai.viletryx) > maxdist
        || doom_abs(thing->y - ai.viletryy) > maxdist)
        return true; // not actually touching

    ai.corpsehit = thing;
    ai.corpsehit->momx = ai.corpsehit->momy = Fixed {};
    ai.corpsehit->height <<= 2;
    bool check = ai.corpsehit->checkPosition(ai.corpsehit->x, ai.corpsehit->y);
    ai.corpsehit->height >>= 2;

    if (!check)
        return true; // doesn't fit here

    return false; // got one, so stop checking
}

//
// vileChase
// Check for ressurecting a body
//
void Mobj::vileChase()
{
    auto& ai = enemyAI();

    if (movedir != toIndex(DirType::NoDir))
    {
        // check for corpses to raise
        ai.viletryx = x + info->speed * xspeed[movedir];
        ai.viletryy = y + info->speed * yspeed[movedir];

        int xl = (ai.viletryx - level().blockmap.origin.x - MAXRADIUS * 2).raw
                 >> MAPBLOCKSHIFT;
        int xh = (ai.viletryx - level().blockmap.origin.x + MAXRADIUS * 2).raw
                 >> MAPBLOCKSHIFT;
        int yl = (ai.viletryy - level().blockmap.origin.y - MAXRADIUS * 2).raw
                 >> MAPBLOCKSHIFT;
        int yh = (ai.viletryy - level().blockmap.origin.y + MAXRADIUS * 2).raw
                 >> MAPBLOCKSHIFT;

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
                    Mobj* temp = target;
                    target = ai.corpsehit;
                    faceTarget();
                    target = temp;

                    setState(StateNum::VileHeal1);
                    startSound(ai.corpsehit, SfxEnum::Slop);
                    MobjInfo* corpseInfo = ai.corpsehit->info;

                    ai.corpsehit->setState(corpseInfo->raisestate);
                    ai.corpsehit->height <<= 2;
                    ai.corpsehit->flags = corpseInfo->flags;
                    ai.corpsehit->health = corpseInfo->spawnhealth;
                    ai.corpsehit->target = nullptr;

                    return;
                }
            }
        }
    }

    // Return to normal attack.
    chase();
}

//
// vileStart
//
void Mobj::vileStart()
{
    startSound(this, SfxEnum::Vilatk);
}

//
// fire
// Keep fire in front of player unless out of sight
//
void Mobj::startFire()
{
    startSound(this, SfxEnum::Flamst);
    fire();
}

void Mobj::fireCrackle()
{
    startSound(this, SfxEnum::Flame);
    fire();
}

void Mobj::fire()
{
    Mobj* dest = tracer;
    if (!dest)
        return;

    // don't move it if the vile lost sight
    if (!checkSight(target, dest))
        return;

    const auto anFine = dest->angle.fineIndex();

    unsetThingPosition(*this);
    x = dest->x + FixedMul(24 * FRACUNIT, finecosine()[anFine]);
    y = dest->y + FixedMul(24 * FRACUNIT, finesine()[anFine]);
    z = dest->z;
    setThingPosition(*this);
}

//
// vileTarget
// Spawn the hellfire
//
void Mobj::vileTarget()
{
    if (!target)
        return;

    faceTarget();

    Mobj* fog = spawnMobj(target->x, target->x, target->z, MobjType::Fire);

    tracer = fog;
    fog->target = this;
    fog->tracer = target;
    fog->fire();
}

//
// vileAttack
//
void Mobj::vileAttack()
{
    if (!target)
        return;

    faceTarget();

    if (!checkSight(this, target))
        return;

    startSound(this, SfxEnum::Barexp);
    target->damage(this, this, 20);
    target->momz = 1000 * FRACUNIT / target->info->mass;

    const auto anFine = angle.fineIndex();

    Mobj* fire = tracer;

    if (!fire)
        return;

    // move the fire between the vile and the player
    fire->x = target->x - FixedMul(24 * FRACUNIT, finecosine()[anFine]);
    fire->y = target->y - FixedMul(24 * FRACUNIT, finesine()[anFine]);
    fire->radiusAttack(this, 70);
}

//
// Mancubus attack,
// firing three missiles (bruisers)
// in three different directions?
// Doesn't look like it.
//
void Mobj::fatRaise()
{
    faceTarget();
    startSound(this, SfxEnum::Manatk);
}

void Mobj::fatAttack1()
{
    faceTarget();
    // Change direction  to ...
    angle += FATSPREAD;
    spawnMissile(target, MobjType::Fatshot);

    Mobj* mo = spawnMissile(target, MobjType::Fatshot);
    mo->angle += FATSPREAD;
    const auto an1Fine = mo->angle.fineIndex();
    mo->momx = FixedMul(Fixed {mo->info->speed}, finecosine()[an1Fine]);
    mo->momy = FixedMul(Fixed {mo->info->speed}, finesine()[an1Fine]);
}

void Mobj::fatAttack2()
{
    faceTarget();
    // Now here choose opposite deviation.
    angle -= FATSPREAD;
    spawnMissile(target, MobjType::Fatshot);

    Mobj* mo = spawnMissile(target, MobjType::Fatshot);
    mo->angle -= FATSPREAD * 2;
    const auto an2Fine = mo->angle.fineIndex();
    mo->momx = FixedMul(Fixed {mo->info->speed}, finecosine()[an2Fine]);
    mo->momy = FixedMul(Fixed {mo->info->speed}, finesine()[an2Fine]);
}

void Mobj::fatAttack3()
{
    faceTarget();

    Mobj* mo = spawnMissile(target, MobjType::Fatshot);
    mo->angle -= FATSPREAD / 2;
    const auto an3Fine = mo->angle.fineIndex();
    mo->momx = FixedMul(Fixed {mo->info->speed}, finecosine()[an3Fine]);
    mo->momy = FixedMul(Fixed {mo->info->speed}, finesine()[an3Fine]);

    mo = spawnMissile(target, MobjType::Fatshot);
    mo->angle += FATSPREAD / 2;
    const auto an4Fine = mo->angle.fineIndex();
    mo->momx = FixedMul(Fixed {mo->info->speed}, finecosine()[an4Fine]);
    mo->momy = FixedMul(Fixed {mo->info->speed}, finesine()[an4Fine]);
}

//
// SkullAttack
// Fly at the player like a missile.
//
void Mobj::skullAttack()
{
    if (!target)
        return;

    Mobj* dest = target;
    flags = withFlags(flags, MobjFlag::SkullFly);

    startSound(this, info->attacksound);
    faceTarget();
    const auto fine = angle.fineIndex();
    momx = FixedMul(SKULLSPEED, finecosine()[fine]);
    momy = FixedMul(SKULLSPEED, finesine()[fine]);
    int dist = approxDistance(dest->x - x, dest->y - y).raw;
    dist = dist / SKULLSPEED.raw;

    if (dist < 1)
        dist = 1;
    momz = (dest->z + (dest->height >> 1) - z) / dist;
}

//
// painShootSkull
// Spawn a lost soul and launch it at the target
//
void Mobj::painShootSkull(Angle angleToUse)
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
    const auto anFine = angleToUse.fineIndex();

    Fixed prestep =
        4 * FRACUNIT
        + 3 * (info->radius + mobjinfo()[toIndex(MobjType::Skull)].radius) / 2;

    Fixed xToUse = x + FixedMul(prestep, finecosine()[anFine]);
    Fixed yToUse = y + FixedMul(prestep, finesine()[anFine]);
    Fixed zToUse = z + 8 * FRACUNIT;

    Mobj* newmobj = spawnMobj(xToUse, yToUse, zToUse, MobjType::Skull);

    // Check for movements.
    if (!newmobj->tryMove(newmobj->x, newmobj->y))
    {
        // kill it immediately
        newmobj->damage(this, this, 10000);
        return;
    }

    newmobj->target = target;
    newmobj->skullAttack();
}

//
// painAttack
// Spawn a lost soul and launch it at the target
//
void Mobj::painAttack()
{
    if (!target)
        return;

    faceTarget();
    painShootSkull(angle);
}

void Mobj::painDie()
{
    fall();
    painShootSkull(angle + ang90);
    painShootSkull(angle + ang180);
    painShootSkull(angle + ang270);
}

void Mobj::scream()
{
    SfxEnum sound;

    switch (info->deathsound)
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
            sound = info->deathsound;
            break;
    }

    // Check for bosses.
    if (type == MobjType::Spider || type == MobjType::Cyborg)
    {
        // full volume
        startSound(0, sound);
    }
    else
        startSound(this, sound);
}

void Mobj::xScream()
{
    startSound(this, SfxEnum::Slop);
}

void Mobj::pain()
{
    if (info->painsound != SfxEnum::None)
        startSound(this, info->painsound);
}

void Mobj::fall()
{
    // it is on the ground, it can be walked over
    flags = withoutFlags(flags, MobjFlag::Solid);

    // So change this if corpse objects
    // are meant to be obstacles.
}

//
// explode
//
void Mobj::explode()
{
    radiusAttack(target, 128);
}

//
// bossDeath
// Possibly trigger special effects
// if on first boss level
//
void Mobj::bossDeath()
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

        if ((type != MobjType::Fatso) && (type != MobjType::Baby))
            return;
    }
    else
    {
        switch (session.gameepisode)
        {
            case 1:
                if (session.gamemap != 8)
                    return;

                if (type != MobjType::Bruiser)
                    return;
                break;

            case 2:
                if (session.gamemap != 8)
                    return;

                if (type != MobjType::Cyborg)
                    return;
                break;

            case 3:
                if (session.gamemap != 8)
                    return;

                if (type != MobjType::Spider)
                    return;

                break;

            case 4:
                switch (session.gamemap)
                {
                    case 6:
                        if (type != MobjType::Cyborg)
                            return;
                        break;

                    case 8:
                        if (type != MobjType::Spider)
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
        if (mo2 != this && mo2->type == type && mo2->health > 0)
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
            if (type == MobjType::Fatso)
            {
                junk.tag = 666;
                junk.doFloor(FloorType::LowerFloorToLowest);
                return;
            }

            if (type == MobjType::Baby)
            {
                junk.tag = 667;
                junk.doFloor(FloorType::RaiseToTexture);
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
                junk.doFloor(FloorType::LowerFloorToLowest);
                return;
                break;

            case 4:
                switch (session.gamemap)
                {
                    case 6:
                        junk.tag = 666;
                        junk.doDoor(DoorType::BlazeOpen);
                        return;
                        break;

                    case 8:
                        junk.tag = 666;
                        junk.doFloor(FloorType::LowerFloorToLowest);
                        return;
                        break;
                }
        }
    }

    exitLevel();
}

void Mobj::hoof()
{
    startSound(this, SfxEnum::Hoof);
    chase();
}

void Mobj::metal()
{
    startSound(this, SfxEnum::Metal);
    chase();
}

void Mobj::babyMetal()
{
    startSound(this, SfxEnum::Bspwlk);
    chase();
}

void Player::openShotgun2(PspDef&)
{
    startSound(mo, SfxEnum::Dbopn);
}

void Player::loadShotgun2(PspDef&)
{
    startSound(mo, SfxEnum::Dbload);
}

void Player::closeShotgun2(PspDef& psp)
{
    startSound(mo, SfxEnum::Dbcls);
    reFire(psp);
}

void Mobj::brainAwake()
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

void Mobj::brainPain()
{
    startSound(0, SfxEnum::Bospn);
}

void Mobj::brainScream()
{
    for (Fixed xToUse = x - 196 * FRACUNIT; xToUse < x + 320 * FRACUNIT;
         xToUse += FRACUNIT * 8)
    {
        Fixed yToUse = y - 320 * FRACUNIT;
        // vanilla's raw 128 added to a whole-unit-scaled random; kept as it stands.
        Fixed zToUse = Fixed {128} + randomness().forPlay() * 2 * FRACUNIT;
        Mobj* th = spawnMobj(xToUse, yToUse, zToUse, MobjType::Rocket);
        th->momz = Fixed {randomness().forPlay() * 512};

        th->setState(StateNum::Brainexplode1);

        th->tics -= randomness().forPlay() & 7;
        if (th->tics < 1)
            th->tics = 1;
    }

    startSound(0, SfxEnum::Bosdth);
}

void Mobj::brainExplode()
{
    Fixed xToUse =
        x + Fixed {(randomness().forPlay() - randomness().forPlay()) * 2048};
    Fixed yToUse = y;
    Fixed zToUse = Fixed {128} + randomness().forPlay() * 2 * FRACUNIT;
    Mobj* th = spawnMobj(xToUse, yToUse, zToUse, MobjType::Rocket);
    th->momz = Fixed {randomness().forPlay() * 512};

    th->setState(StateNum::Brainexplode1);

    th->tics -= randomness().forPlay() & 7;
    if (th->tics < 1)
        th->tics = 1;
}

void Mobj::brainDie()
{
    exitLevel();
}

void Mobj::brainSpit()
{
    auto& ai = enemyAI();

    ai.brainSpitEasy ^= 1;
    if (gameSession().gameskill <= Skill::Easy && (!ai.brainSpitEasy))
        return;

    // shoot a cube at current target
    Mobj* targ = ai.braintargets[ai.braintargeton];
    ai.braintargeton = (ai.braintargeton + 1) % ai.braintargets.size();

    // spawn brain missile
    Mobj* newmobj = spawnMissile(targ, MobjType::Spawnshot);
    newmobj->target = targ;
    // Vanilla divides the raw values as plain integers here - the result is a tic
    // count, not a length. A fixed-point divide would scale it by 65536.
    newmobj->reactiontime =
        ((targ->y - y).raw / newmobj->momy.raw) / newmobj->state->tics;

    startSound(0, SfxEnum::Bospit);
}

// travelling cube sound
void Mobj::spawnSound()
{
    startSound(this, SfxEnum::Boscub);
    spawnFly();
}

void Mobj::spawnFly()
{
    MobjType spawnType;

    if (--reactiontime)
        return; // still flying

    Mobj* targ = target;

    // First spawn teleport fog.
    Mobj* fog = spawnMobj(targ->x, targ->y, targ->z, MobjType::Spawnfire);
    startSound(fog, SfxEnum::Telept);

    // Randomly select monster to spawn.
    int r = randomness().forPlay();

    // Probability distribution (kind of :),
    // decreasing likelihood.
    if (r < 50)
        spawnType = MobjType::Troop;
    else if (r < 90)
        spawnType = MobjType::Sergeant;
    else if (r < 120)
        spawnType = MobjType::Shadows;
    else if (r < 130)
        spawnType = MobjType::Pain;
    else if (r < 160)
        spawnType = MobjType::Head;
    else if (r < 162)
        spawnType = MobjType::Vile;
    else if (r < 172)
        spawnType = MobjType::Undead;
    else if (r < 192)
        spawnType = MobjType::Baby;
    else if (r < 222)
        spawnType = MobjType::Fatso;
    else if (r < 246)
        spawnType = MobjType::Knight;
    else
        spawnType = MobjType::Bruiser;

    Mobj* newmobj = spawnMobj(targ->x, targ->y, targ->z, spawnType);
    if (newmobj->lookForPlayers(true))
        newmobj->setState(static_cast<StateNum>(newmobj->info->seestate));

    // telefrag anything in this spot
    newmobj->teleportMove(newmobj->x, newmobj->y);

    // remove self (i.e., cube).
    remove();
}

void Mobj::playerScream()
{
    // Default death sound.
    SfxEnum sound = SfxEnum::Pldeth;

    if ((gameVersion().gamemode == GameMode::Commercial) && (health < -50))
    {
        // IF THE PLAYER DIES
        // LESS THAN -50% WITHOUT GIBBING
        sound = SfxEnum::Pdiehi;
    }

    startSound(this, sound);
}
} // namespace Doom
