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

#include <ea_data_structures/Structures/Array.h>

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
#define MAXSPECIALCROSS 8

namespace Doom
{
constexpr angle_t FATSPREAD = ang90 / 8;
constexpr fixed_t SKULLSPEED = 20 * FRACUNIT;

// P_NewChaseDir movement LUTs and the transient targets the AI threads through its
// state actions (the vile's corpse, the fat/brain spit targets). All file-local;
// soundtarget alone is shared (now a Doom::SoundTarget Engine member, reached above).
enum DirType
{
    DI_EAST,
    DI_NORTHEAST,
    DI_NORTH,
    DI_NORTHWEST,
    DI_WEST,
    DI_SOUTHWEST,
    DI_SOUTH,
    DI_SOUTHEAST,
    DI_NODIR,
    NUMDIRS
};

EA::Array<DirType, 9> opposite = {DI_WEST,
                                  DI_SOUTHWEST,
                                  DI_SOUTH,
                                  DI_SOUTHEAST,
                                  DI_EAST,
                                  DI_NORTHEAST,
                                  DI_NORTH,
                                  DI_NORTHWEST,
                                  DI_NODIR};

EA::Array<DirType, 4> diags = {
    DI_NORTHWEST, DI_NORTHEAST, DI_SOUTHWEST, DI_SOUTHEAST};

// Raw fixed values: 47000 is FRACUNIT * cos(45) rounded, as vanilla wrote it.
EA::Array<fixed_t, 8> xspeed = {FRACUNIT,
                                fixed_t {47000},
                                fixed_t {},
                                fixed_t {-47000},
                                -FRACUNIT,
                                fixed_t {-47000},
                                fixed_t {},
                                fixed_t {47000}};
EA::Array<fixed_t, 8> yspeed = {fixed_t {},
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
void noiseAlert(Mobj* target, Mobj& emmiter);
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
void openShotgun2(Player* player, PspDef* psp);
void loadShotgun2(Player* player, PspDef* psp);
void closeShotgun2(Player* player, PspDef* psp);
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
    Line* check;
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
        check = sec->lines[i];
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
void noiseAlert(Mobj* target, Mobj& emmiter)
{
    soundTarget().soundtarget = target;
    validCount().validcount++;
    recursiveSound(emmiter.subsector->sector, 0);
}

//
// checkMeleeRange
//
bool checkMeleeRange(Mobj& actor)
{
    Mobj* pl;
    fixed_t dist;

    if (!actor.target)
        return false;

    pl = actor.target;
    dist = approxDistance(pl->x - actor.x, pl->y - actor.y);

    if (dist >= MELEERANGE - 20 * FRACUNIT + pl->info->radius)
        return false;

    if (!Doom::checkSight(&actor, actor.target))
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
    fixed_t distance;
    int dist;

    if (!Doom::checkSight(&actor, actor.target))
        return false;

    if (actor.flags & MF_JUSTHIT)
    {
        // the target just hit the enemy,
        // so fight back!
        actor.flags &= ~MF_JUSTHIT;
        return true;
    }

    if (actor.reactiontime)
        return false; // do not attack yet

    // OPTIMIZE: get this from a global checksight
    distance = approxDistance(actor.x - actor.target->x, actor.y - actor.target->y)
               - 64 * FRACUNIT;

    if (!actor.info->meleestate)
        distance -= 128 * FRACUNIT; // no melee attack, so fire more

    dist = distance.toInt();

    if (actor.type == MT_VILE)
    {
        if (dist > 14 * 64)
            return false; // too far away
    }

    if (actor.type == MT_UNDEAD)
    {
        if (dist < 196)
            return false; // close for fist attack
        dist >>= 1;
    }

    if (actor.type == MT_CYBORG || actor.type == MT_SPIDER || actor.type == MT_SKULL)
    {
        dist >>= 1;
    }

    if (dist > 200)
        dist = 200;

    if (actor.type == MT_CYBORG && dist > 160)
        dist = 160;

    if (Doom::randomness().forPlay() < dist)
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
    fixed_t tryx;
    fixed_t tryy;

    Line* ld;

    // warning: 'catch', 'throw', and 'try'
    // are all C++ reserved words
    bool try_ok;
    bool good;

    auto& c = clip();

    if (actor.movedir == DI_NODIR)
        return false;

    if (static_cast<unsigned>(actor.movedir) >= 8)
        fatalError("Error: Weird actor->movedir!");

    tryx = actor.x + actor.info->speed * xspeed[actor.movedir];
    tryy = actor.y + actor.info->speed * yspeed[actor.movedir];

    try_ok = Doom::tryMove(&actor, tryx, tryy);

    if (!try_ok)
    {
        // open any specials
        if (actor.flags & MF_FLOAT && c.floatok)
        {
            // must adjust height
            if (actor.z < c.tmfloorz)
                actor.z += FLOATSPEED;
            else
                actor.z -= FLOATSPEED;

            actor.flags |= MF_INFLOAT;
            return true;
        }

        if (!c.numspechit)
            return false;

        actor.movedir = DI_NODIR;
        good = false;
        while (c.numspechit--)
        {
            ld = c.spechit[c.numspechit];
            // if the special is not a door
            // that can be opened,
            // return false
            if (Doom::useSpecialLine(&actor, ld, 0))
                good = true;
        }
        return good;
    }
    else
    {
        actor.flags &= ~MF_INFLOAT;
    }

    if (!(actor.flags & MF_FLOAT))
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

    actor.movecount = Doom::randomness().forPlay() & 15;
    return true;
}

void newChaseDir(Mobj& actor)
{
    fixed_t deltax;
    fixed_t deltay;

    EA::Array<DirType, 3> d;

    int tdir;
    DirType olddir;

    DirType turnaround;

    if (!actor.target)
        fatalError("Error: newChaseDir: called with no target");

    olddir = static_cast<DirType>(actor.movedir);
    turnaround = opposite[olddir];

    deltax = actor.target->x - actor.x;
    deltay = actor.target->y - actor.y;

    if (deltax > 10 * FRACUNIT)
        d[1] = DI_EAST;
    else if (deltax < -10 * FRACUNIT)
        d[1] = DI_WEST;
    else
        d[1] = DI_NODIR;

    if (deltay < -10 * FRACUNIT)
        d[2] = DI_SOUTH;
    else if (deltay > 10 * FRACUNIT)
        d[2] = DI_NORTH;
    else
        d[2] = DI_NODIR;

    // try direct route
    if (d[1] != DI_NODIR && d[2] != DI_NODIR)
    {
        actor.movedir = diags[(deltay.isNegative() << 1) + deltax.isPositive()];
        if (actor.movedir != turnaround && tryWalk(actor))
            return;
    }

    // try other directions
    if (Doom::randomness().forPlay() > 200 || doom_abs(deltay) > doom_abs(deltax))
    {
        tdir = d[1];
        d[1] = d[2];
        d[2] = static_cast<DirType>(tdir);
    }

    if (d[1] == turnaround)
        d[1] = DI_NODIR;
    if (d[2] == turnaround)
        d[2] = DI_NODIR;

    if (d[1] != DI_NODIR)
    {
        actor.movedir = d[1];
        if (tryWalk(actor))
        {
            // either moved forward or attacked
            return;
        }
    }

    if (d[2] != DI_NODIR)
    {
        actor.movedir = d[2];

        if (tryWalk(actor))
            return;
    }

    // there is no direct path to the player,
    // so pick another direction.
    if (olddir != DI_NODIR)
    {
        actor.movedir = olddir;

        if (tryWalk(actor))
            return;
    }

    // randomly determine direction of search
    if (Doom::randomness().forPlay() & 1)
    {
        for (tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++)
        {
            if (tdir != turnaround)
            {
                actor.movedir = tdir;

                if (tryWalk(actor))
                    return;
            }
        }
    }
    else
    {
        for (tdir = DI_SOUTHEAST; tdir != (DI_EAST - 1); tdir--)
        {
            if (tdir != turnaround)
            {
                actor.movedir = tdir;

                if (tryWalk(actor))
                    return;
            }
        }
    }

    if (turnaround != DI_NODIR)
    {
        actor.movedir = turnaround;
        if (tryWalk(actor))
            return;
    }

    actor.movedir = DI_NODIR; // can not move
}

//
// lookForPlayers
// If allaround is false, only look 180 degrees in front.
// Returns true if a player is targeted.
//
bool lookForPlayers(Mobj& actor, bool allaround)
{
    int c;
    int stop;
    Player* player;
    angle_t an;
    fixed_t dist;

    c = 0;
    stop = (actor.lastlook - 1) & 3;

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

        player = &players_.players[actor.lastlook];

        if (player->health <= 0)
            continue; // dead

        if (!Doom::checkSight(&actor, player->mo))
            continue; // out of sight

        if (!allaround)
        {
            an = Doom::pointToAngle2(actor.x, actor.y, player->mo->x, player->mo->y)
                 - actor.angle;

            if (an > ang90 && an < ang270)
            {
                dist =
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
    Doom::Thinker* th;
    Mobj* mo2;
    Line junk;

    auto& thinkers = thinkerList();

    fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for (th = thinkers.cap.next; th != &thinkers.cap; th = th->next)
    {
        if (th->kind() != Doom::ThinkerKind::Mobj || th->removed)
            continue;

        mo2 = reinterpret_cast<Mobj*>(th);
        if (mo2 != &mo && mo2->type == mo.type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    junk.tag = 666;
    Doom::doDoor(&junk, door_open);
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
    Mobj* targ;

    actor.threshold = 0; // any shot will wake up
    targ = actor.subsector->sector->soundtarget;

    if (targ && (targ->flags & MF_SHOOTABLE))
    {
        actor.target = targ;

        if (actor.flags & MF_AMBUSH)
        {
            if (Doom::checkSight(&actor, actor.target))
                goto seeyou;
        }
        else
            goto seeyou;
    }

    if (!lookForPlayers(actor, false))
        return;

    // go into chase state
seeyou:
    if (actor.info->seesound)
    {
        int sound;

        switch (actor.info->seesound)
        {
            case sfx_posit1:
            case sfx_posit2:
            case sfx_posit3:
                sound = sfx_posit1 + Doom::randomness().forPlay() % 3;
                break;

            case sfx_bgsit1:
            case sfx_bgsit2:
                sound = sfx_bgsit1 + Doom::randomness().forPlay() % 2;
                break;

            default:
                sound = actor.info->seesound;
                break;
        }

        if (actor.type == MT_SPIDER || actor.type == MT_CYBORG)
        {
            // full volume
            Doom::startSound(0, sound);
        }
        else
            Doom::startSound(&actor, sound);
    }

    Doom::setMobjState(&actor, static_cast<StateNum>(actor.info->seestate));
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
    int delta;

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
        delta = (int) (actor.angle - angle_t {(unsigned) actor.movedir << 29}).raw;

        if (delta > 0)
            actor.angle -= ang90 / 2u;
        else if (delta < 0)
            actor.angle += ang90 / 2u;
    }

    if (!actor.target || !(actor.target->flags & MF_SHOOTABLE))
    {
        // look for a new target
        if (lookForPlayers(actor, true))
            return; // got a new target

        Doom::setMobjState(&actor, static_cast<StateNum>(actor.info->spawnstate));
        return;
    }

    const auto& session = gameSession();
    const auto& opts = launchOptions();

    // do not attack twice in a row
    if (actor.flags & MF_JUSTATTACKED)
    {
        actor.flags &= ~MF_JUSTATTACKED;
        if (session.gameskill != sk_nightmare && !opts.fastparm)
            newChaseDir(actor);
        return;
    }

    // check for melee attack
    if (actor.info->meleestate && checkMeleeRange(actor))
    {
        if (actor.info->attacksound)
            Doom::startSound(&actor, actor.info->attacksound);

        Doom::setMobjState(&actor, static_cast<StateNum>(actor.info->meleestate));
        return;
    }

    // check for missile attack
    if (actor.info->missilestate)
    {
        if (session.gameskill < sk_nightmare && !opts.fastparm && actor.movecount)
        {
            goto nomissile;
        }

        if (!checkMissileRange(actor))
            goto nomissile;

        Doom::setMobjState(&actor, static_cast<StateNum>(actor.info->missilestate));
        actor.flags |= MF_JUSTATTACKED;
        return;
    }

    // ?
nomissile:
    // possibly choose another target
    if (session.netgame && !actor.threshold
        && !Doom::checkSight(&actor, actor.target))
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
    if (actor.info->activesound && Doom::randomness().forPlay() < 3)
    {
        Doom::startSound(&actor, actor.info->activesound);
    }
}

//
// faceTarget
//
void faceTarget(Mobj& actor)
{
    if (!actor.target)
        return;

    actor.flags &= ~MF_AMBUSH;

    actor.angle =
        Doom::pointToAngle2(actor.x, actor.y, actor.target->x, actor.target->y);

    if (actor.target->flags & MF_SHADOW)
        actor.angle += angle_t {
            (unsigned) (Doom::randomness().forPlay() - Doom::randomness().forPlay())
            << 21};
}

//
// posAttack
//
void posAttack(Mobj& actor)
{
    angle_t angle;
    int damage;
    fixed_t slope;

    if (!actor.target)
        return;

    faceTarget(actor);
    angle = actor.angle;
    slope = Doom::aimLineAttack(&actor, angle, MISSILERANGE).slope;

    Doom::startSound(&actor, sfx_pistol);
    angle += angle_t {
        (unsigned) (Doom::randomness().forPlay() - Doom::randomness().forPlay())
        << 20};
    damage = ((Doom::randomness().forPlay() % 5) + 1) * 3;
    Doom::lineAttack(&actor, angle, MISSILERANGE, slope, damage);
}

void sPosAttack(Mobj& actor)
{
    angle_t angle {};
    angle_t bangle;
    int damage;
    fixed_t slope;

    if (!actor.target)
        return;

    Doom::startSound(&actor, sfx_shotgn);
    faceTarget(actor);
    bangle = actor.angle;
    slope = Doom::aimLineAttack(&actor, bangle, MISSILERANGE).slope;

    for (int i = 0; i < 3; i++)
    {
        angle = bangle
                + angle_t {(unsigned) (Doom::randomness().forPlay()
                                       - Doom::randomness().forPlay())
                           << 20};
        damage = ((Doom::randomness().forPlay() % 5) + 1) * 3;
        Doom::lineAttack(&actor, angle, MISSILERANGE, slope, damage);
    }
}

void cPosAttack(Mobj& actor)
{
    angle_t angle {};
    angle_t bangle;
    int damage;
    fixed_t slope;

    if (!actor.target)
        return;

    Doom::startSound(&actor, sfx_shotgn);
    faceTarget(actor);
    bangle = actor.angle;
    slope = Doom::aimLineAttack(&actor, bangle, MISSILERANGE).slope;

    angle =
        bangle
        + angle_t {
            (unsigned) (Doom::randomness().forPlay() - Doom::randomness().forPlay())
            << 20};
    damage = ((Doom::randomness().forPlay() % 5) + 1) * 3;
    Doom::lineAttack(&actor, angle, MISSILERANGE, slope, damage);
}

void cPosRefire(Mobj& actor)
{
    // keep firing unless target got out of sight
    faceTarget(actor);

    if (Doom::randomness().forPlay() < 40)
        return;

    if (!actor.target || actor.target->health <= 0
        || !Doom::checkSight(&actor, actor.target))
    {
        Doom::setMobjState(&actor, static_cast<StateNum>(actor.info->seestate));
    }
}

void spidRefire(Mobj& actor)
{
    // keep firing unless target got out of sight
    faceTarget(actor);

    if (Doom::randomness().forPlay() < 10)
        return;

    if (!actor.target || actor.target->health <= 0
        || !Doom::checkSight(&actor, actor.target))
    {
        Doom::setMobjState(&actor, static_cast<StateNum>(actor.info->seestate));
    }
}

void bspiAttack(Mobj& actor)
{
    if (!actor.target)
        return;

    faceTarget(actor);

    // launch a missile
    Doom::spawnMissile(&actor, actor.target, MT_ARACHPLAZ);
}

//
// troopAttack
//
void troopAttack(Mobj& actor)
{
    int damage;

    if (!actor.target)
        return;

    faceTarget(actor);
    if (checkMeleeRange(actor))
    {
        Doom::startSound(&actor, sfx_claw);
        damage = (Doom::randomness().forPlay() % 8 + 1) * 3;
        Doom::damageMobj(actor.target, &actor, &actor, damage);
        return;
    }

    // launch a missile
    Doom::spawnMissile(&actor, actor.target, MT_TROOPSHOT);
}

void sargAttack(Mobj& actor)
{
    int damage;

    if (!actor.target)
        return;

    faceTarget(actor);
    if (checkMeleeRange(actor))
    {
        damage = ((Doom::randomness().forPlay() % 10) + 1) * 4;
        Doom::damageMobj(actor.target, &actor, &actor, damage);
    }
}

void headAttack(Mobj& actor)
{
    int damage;

    if (!actor.target)
        return;

    faceTarget(actor);
    if (checkMeleeRange(actor))
    {
        damage = (Doom::randomness().forPlay() % 6 + 1) * 10;
        Doom::damageMobj(actor.target, &actor, &actor, damage);
        return;
    }

    // launch a missile
    Doom::spawnMissile(&actor, actor.target, MT_HEADSHOT);
}

void cyberAttack(Mobj& actor)
{
    if (!actor.target)
        return;

    faceTarget(actor);
    Doom::spawnMissile(&actor, actor.target, MT_ROCKET);
}

void bruisAttack(Mobj& actor)
{
    int damage;

    if (!actor.target)
        return;

    if (checkMeleeRange(actor))
    {
        Doom::startSound(&actor, sfx_claw);
        damage = (Doom::randomness().forPlay() % 8 + 1) * 10;
        Doom::damageMobj(actor.target, &actor, &actor, damage);
        return;
    }

    // launch a missile
    Doom::spawnMissile(&actor, actor.target, MT_BRUISERSHOT);
}

//
// skelMissile
//
void skelMissile(Mobj& actor)
{
    Mobj* mo;

    if (!actor.target)
        return;

    faceTarget(actor);
    actor.z += 16 * FRACUNIT; // so missile spawns higher
    mo = Doom::spawnMissile(&actor, actor.target, MT_TRACER);
    actor.z -= 16 * FRACUNIT; // back to normal

    mo->x += mo->momx;
    mo->y += mo->momy;
    mo->tracer = actor.target;
}

void tracer(Mobj& actor)
{
    angle_t exact;
    // As in spawnMissile: dist is a tic count once divided by the missile's raw
    // speed, and is then the plain integer divisor of the height difference.
    int dist;
    fixed_t slope;
    Mobj* dest;
    Mobj* th;

    if (gameClock().gametic & 3)
        return;

    // spawn a puff of smoke behind the rocket
    Doom::spawnPuff(actor.x, actor.y, actor.z);

    th = Doom::spawnMobj(
        actor.x - actor.momx, actor.y - actor.momy, actor.z, MT_SMOKE);

    th->momz = FRACUNIT;
    th->tics -= Doom::randomness().forPlay() & 3;
    if (th->tics < 1)
        th->tics = 1;

    // adjust direction
    dest = actor.tracer;

    if (!dest || dest->health <= 0)
        return;

    // change angle
    exact = Doom::pointToAngle2(actor.x, actor.y, dest->x, dest->y);

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
    actor.momx = FixedMul(Doom::Fixed {actor.info->speed}, finecosine[fine]);
    actor.momy = FixedMul(Doom::Fixed {actor.info->speed}, finesine[fine]);

    // change slope
    dist = approxDistance(dest->x - actor.x, dest->y - actor.y).raw;

    dist = dist / actor.info->speed;

    if (dist < 1)
        dist = 1;
    slope = (dest->z + 40 * FRACUNIT - actor.z) / dist;

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
    Doom::startSound(&actor, sfx_skeswg);
}

void skelFist(Mobj& actor)
{
    int damage;

    if (!actor.target)
        return;

    faceTarget(actor);

    if (checkMeleeRange(actor))
    {
        damage = ((Doom::randomness().forPlay() % 10) + 1) * 6;
        Doom::startSound(&actor, sfx_skepch);
        Doom::damageMobj(actor.target, &actor, &actor, damage);
    }
}

//
// vileCheck
// Detect a corpse that could be raised.
//
bool vileCheck(Mobj* thing)
{
    fixed_t maxdist;
    bool check;

    auto& ai = enemyAI();

    if (!(thing->flags & MF_CORPSE))
        return true; // not a monster

    if (thing->tics != -1)
        return true; // not lying still yet

    if (thing->info->raisestate == S_NULL)
        return true; // monster doesn't have a raise state

    maxdist = thing->info->radius + mobjinfo[MT_VILE].radius;

    if (doom_abs(thing->x - ai.viletryx) > maxdist
        || doom_abs(thing->y - ai.viletryy) > maxdist)
        return true; // not actually touching

    ai.corpsehit = thing;
    ai.corpsehit->momx = ai.corpsehit->momy = fixed_t {};
    ai.corpsehit->height <<= 2;
    check = Doom::checkPosition(ai.corpsehit, ai.corpsehit->x, ai.corpsehit->y);
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
    int xl;
    int xh;
    int yl;
    int yh;

    MobjInfo* info;
    Mobj* temp;

    auto& ai = enemyAI();

    if (actor.movedir != DI_NODIR)
    {
        // check for corpses to raise
        ai.viletryx = actor.x + actor.info->speed * xspeed[actor.movedir];
        ai.viletryy = actor.y + actor.info->speed * yspeed[actor.movedir];

        xl = (ai.viletryx - bmaporgx - MAXRADIUS * 2).raw >> MAPBLOCKSHIFT;
        xh = (ai.viletryx - bmaporgx + MAXRADIUS * 2).raw >> MAPBLOCKSHIFT;
        yl = (ai.viletryy - bmaporgy - MAXRADIUS * 2).raw >> MAPBLOCKSHIFT;
        yh = (ai.viletryy - bmaporgy + MAXRADIUS * 2).raw >> MAPBLOCKSHIFT;

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
                    temp = actor.target;
                    actor.target = ai.corpsehit;
                    faceTarget(actor);
                    actor.target = temp;

                    Doom::setMobjState(&actor, S_VILE_HEAL1);
                    Doom::startSound(ai.corpsehit, sfx_slop);
                    info = ai.corpsehit->info;

                    Doom::setMobjState(ai.corpsehit,
                                       static_cast<StateNum>(info->raisestate));
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
    Doom::startSound(&actor, sfx_vilatk);
}

//
// fire
// Keep fire in front of player unless out of sight
//
void startFire(Mobj& actor)
{
    Doom::startSound(&actor, sfx_flamst);
    fire(actor);
}

void fireCrackle(Mobj& actor)
{
    Doom::startSound(&actor, sfx_flame);
    fire(actor);
}

void fire(Mobj& actor)
{
    Mobj* dest;

    dest = actor.tracer;
    if (!dest)
        return;

    // don't move it if the vile lost sight
    if (!Doom::checkSight(actor.target, dest))
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
    Mobj* fog;

    if (!actor.target)
        return;

    faceTarget(actor);

    fog =
        Doom::spawnMobj(actor.target->x, actor.target->x, actor.target->z, MT_FIRE);

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
    Mobj* fire;

    if (!actor.target)
        return;

    faceTarget(actor);

    if (!Doom::checkSight(&actor, actor.target))
        return;

    Doom::startSound(&actor, sfx_barexp);
    Doom::damageMobj(actor.target, &actor, &actor, 20);
    actor.target->momz = 1000 * FRACUNIT / actor.target->info->mass;

    const auto anFine = actor.angle.fineIndex();

    fire = actor.tracer;

    if (!fire)
        return;

    // move the fire between the vile and the player
    fire->x = actor.target->x - FixedMul(24 * FRACUNIT, finecosine[anFine]);
    fire->y = actor.target->y - FixedMul(24 * FRACUNIT, finesine[anFine]);
    Doom::radiusAttack(fire, &actor, 70);
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
    Doom::startSound(&actor, sfx_manatk);
}

void fatAttack1(Mobj& actor)
{
    Mobj* mo;

    faceTarget(actor);
    // Change direction  to ...
    actor.angle += FATSPREAD;
    Doom::spawnMissile(&actor, actor.target, MT_FATSHOT);

    mo = Doom::spawnMissile(&actor, actor.target, MT_FATSHOT);
    mo->angle += FATSPREAD;
    const auto an1Fine = mo->angle.fineIndex();
    mo->momx = FixedMul(Doom::Fixed {mo->info->speed}, finecosine[an1Fine]);
    mo->momy = FixedMul(Doom::Fixed {mo->info->speed}, finesine[an1Fine]);
}

void fatAttack2(Mobj& actor)
{
    Mobj* mo;

    faceTarget(actor);
    // Now here choose opposite deviation.
    actor.angle -= FATSPREAD;
    Doom::spawnMissile(&actor, actor.target, MT_FATSHOT);

    mo = Doom::spawnMissile(&actor, actor.target, MT_FATSHOT);
    mo->angle -= FATSPREAD * 2;
    const auto an2Fine = mo->angle.fineIndex();
    mo->momx = FixedMul(Doom::Fixed {mo->info->speed}, finecosine[an2Fine]);
    mo->momy = FixedMul(Doom::Fixed {mo->info->speed}, finesine[an2Fine]);
}

void fatAttack3(Mobj& actor)
{
    Mobj* mo;

    faceTarget(actor);

    mo = Doom::spawnMissile(&actor, actor.target, MT_FATSHOT);
    mo->angle -= FATSPREAD / 2;
    const auto an3Fine = mo->angle.fineIndex();
    mo->momx = FixedMul(Doom::Fixed {mo->info->speed}, finecosine[an3Fine]);
    mo->momy = FixedMul(Doom::Fixed {mo->info->speed}, finesine[an3Fine]);

    mo = Doom::spawnMissile(&actor, actor.target, MT_FATSHOT);
    mo->angle += FATSPREAD / 2;
    const auto an4Fine = mo->angle.fineIndex();
    mo->momx = FixedMul(Doom::Fixed {mo->info->speed}, finecosine[an4Fine]);
    mo->momy = FixedMul(Doom::Fixed {mo->info->speed}, finesine[an4Fine]);
}

//
// SkullAttack
// Fly at the player like a missile.
//
void skullAttack(Mobj& actor)
{
    Mobj* dest;
    int dist;

    if (!actor.target)
        return;

    dest = actor.target;
    actor.flags |= MF_SKULLFLY;

    Doom::startSound(&actor, actor.info->attacksound);
    faceTarget(actor);
    const auto fine = actor.angle.fineIndex();
    actor.momx = FixedMul(SKULLSPEED, finecosine[fine]);
    actor.momy = FixedMul(SKULLSPEED, finesine[fine]);
    dist = approxDistance(dest->x - actor.x, dest->y - actor.y).raw;
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
    fixed_t x;
    fixed_t y;
    fixed_t z;

    Mobj* newmobj;
    fixed_t prestep;
    int count;
    Doom::Thinker* currentthinker;

    auto& thinkers = thinkerList();

    // count total number of skull currently on the level
    count = 0;

    currentthinker = thinkers.cap.next;
    while (currentthinker != &thinkers.cap)
    {
        if (currentthinker->kind() == Doom::ThinkerKind::Mobj
            && !currentthinker->removed
            && (reinterpret_cast<Mobj*>(currentthinker))->type == MT_SKULL)
            count++;
        currentthinker = currentthinker->next;
    }

    // if there are allready 20 skulls on the level,
    // don't spit another one
    if (count > 20)
        return;

    // okay, there's playe for another one
    const auto anFine = angle.fineIndex();

    prestep =
        4 * FRACUNIT + 3 * (actor.info->radius + mobjinfo[MT_SKULL].radius) / 2;

    x = actor.x + FixedMul(prestep, finecosine[anFine]);
    y = actor.y + FixedMul(prestep, finesine[anFine]);
    z = actor.z + 8 * FRACUNIT;

    newmobj = Doom::spawnMobj(x, y, z, MT_SKULL);

    // Check for movements.
    if (!Doom::tryMove(newmobj, newmobj->x, newmobj->y))
    {
        // kill it immediately
        Doom::damageMobj(newmobj, &actor, &actor, 10000);
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
    int sound;

    switch (actor.info->deathsound)
    {
        case 0:
            return;

        case sfx_podth1:
        case sfx_podth2:
        case sfx_podth3:
            sound = sfx_podth1 + Doom::randomness().forPlay() % 3;
            break;

        case sfx_bgdth1:
        case sfx_bgdth2:
            sound = sfx_bgdth1 + Doom::randomness().forPlay() % 2;
            break;

        default:
            sound = actor.info->deathsound;
            break;
    }

    // Check for bosses.
    if (actor.type == MT_SPIDER || actor.type == MT_CYBORG)
    {
        // full volume
        Doom::startSound(0, sound);
    }
    else
        Doom::startSound(&actor, sound);
}

void xScream(Mobj& actor)
{
    Doom::startSound(&actor, sfx_slop);
}

void pain(Mobj& actor)
{
    if (actor.info->painsound)
        Doom::startSound(&actor, actor.info->painsound);
}

void fall(Mobj& actor)
{
    // actor is on ground, it can be walked over
    actor.flags &= ~MF_SOLID;

    // So change this if corpse objects
    // are meant to be obstacles.
}

//
// explode
//
void explode(Mobj& thingy)
{
    Doom::radiusAttack(&thingy, thingy.target, 128);
}

//
// bossDeath
// Possibly trigger special effects
// if on first boss level
//
void bossDeath(Mobj& mo)
{
    Doom::Thinker* th;
    Mobj* mo2;
    Line junk;
    int i;

    auto& thinkers = thinkerList();
    const auto& version = gameVersion();
    const auto& session = gameSession();
    const auto& players_ = playerState();

    if (version.gamemode == commercial)
    {
        if (session.gamemap != 7)
            return;

        if ((mo.type != MT_FATSO) && (mo.type != MT_BABY))
            return;
    }
    else
    {
        switch (session.gameepisode)
        {
            case 1:
                if (session.gamemap != 8)
                    return;

                if (mo.type != MT_BRUISER)
                    return;
                break;

            case 2:
                if (session.gamemap != 8)
                    return;

                if (mo.type != MT_CYBORG)
                    return;
                break;

            case 3:
                if (session.gamemap != 8)
                    return;

                if (mo.type != MT_SPIDER)
                    return;

                break;

            case 4:
                switch (session.gamemap)
                {
                    case 6:
                        if (mo.type != MT_CYBORG)
                            return;
                        break;

                    case 8:
                        if (mo.type != MT_SPIDER)
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
    for (i = 0; i < MAXPLAYERS; i++)
        if (players_.playeringame[i] && players_.players[i].health > 0)
            break;

    if (i == MAXPLAYERS)
        return; // no one left alive, so do not end game

    // scan the remaining thinkers to see
    // if all bosses are dead
    for (th = thinkers.cap.next; th != &thinkers.cap; th = th->next)
    {
        if (th->kind() != Doom::ThinkerKind::Mobj || th->removed)
            continue;

        mo2 = reinterpret_cast<Mobj*>(th);
        if (mo2 != &mo && mo2->type == mo.type && mo2->health > 0)
        {
            // other boss not dead
            return;
        }
    }

    // victory!
    if (version.gamemode == commercial)
    {
        if (session.gamemap == 7)
        {
            if (mo.type == MT_FATSO)
            {
                junk.tag = 666;
                Doom::doFloor(&junk, lowerFloorToLowest);
                return;
            }

            if (mo.type == MT_BABY)
            {
                junk.tag = 667;
                Doom::doFloor(&junk, raiseToTexture);
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
                Doom::doFloor(&junk, lowerFloorToLowest);
                return;
                break;

            case 4:
                switch (session.gamemap)
                {
                    case 6:
                        junk.tag = 666;
                        Doom::doDoor(&junk, blazeOpen);
                        return;
                        break;

                    case 8:
                        junk.tag = 666;
                        Doom::doFloor(&junk, lowerFloorToLowest);
                        return;
                        break;
                }
        }
    }

    Doom::exitLevel();
}

void hoof(Mobj& mo)
{
    Doom::startSound(&mo, sfx_hoof);
    chase(mo);
}

void metal(Mobj& mo)
{
    Doom::startSound(&mo, sfx_metal);
    chase(mo);
}

void babyMetal(Mobj& mo)
{
    Doom::startSound(&mo, sfx_bspwlk);
    chase(mo);
}

void openShotgun2(Player* player, PspDef*)
{
    Doom::startSound(player->mo, sfx_dbopn);
}

void loadShotgun2(Player* player, PspDef*)
{
    Doom::startSound(player->mo, sfx_dbload);
}

void closeShotgun2(Player* player, PspDef* psp)
{
    Doom::startSound(player->mo, sfx_dbcls);
    Doom::reFire(*player, *psp);
}

void brainAwake(Mobj&)
{
    Doom::Thinker* thinker;
    Mobj* m;

    auto& thinkers = thinkerList();
    auto& ai = enemyAI();

    // find all the target spots
    ai.numbraintargets = 0;
    ai.braintargeton = 0;

    thinker = thinkers.cap.next;
    for (thinker = thinkers.cap.next; thinker != &thinkers.cap;
         thinker = thinker->next)
    {
        if (thinker->kind() != Doom::ThinkerKind::Mobj || thinker->removed)
            continue; // not a mobj

        m = reinterpret_cast<Mobj*>(thinker);

        if (m->type == MT_BOSSTARGET)
        {
            ai.braintargets[ai.numbraintargets] = m;
            ai.numbraintargets++;
        }
    }

    Doom::startSound(0, sfx_bossit);
}

void brainPain(Mobj&)
{
    Doom::startSound(0, sfx_bospn);
}

void brainScream(Mobj& mo)
{
    fixed_t y;
    fixed_t z;
    Mobj* th;

    for (fixed_t x = mo.x - 196 * FRACUNIT; x < mo.x + 320 * FRACUNIT;
         x += FRACUNIT * 8)
    {
        y = mo.y - 320 * FRACUNIT;
        // vanilla's raw 128 added to a whole-unit-scaled random; kept as it stands.
        z = Doom::Fixed {128} + Doom::randomness().forPlay() * 2 * FRACUNIT;
        th = Doom::spawnMobj(x, y, z, MT_ROCKET);
        th->momz = Doom::Fixed {Doom::randomness().forPlay() * 512};

        Doom::setMobjState(th, S_BRAINEXPLODE1);

        th->tics -= Doom::randomness().forPlay() & 7;
        if (th->tics < 1)
            th->tics = 1;
    }

    Doom::startSound(0, sfx_bosdth);
}

void brainExplode(Mobj& mo)
{
    fixed_t x;
    fixed_t y;
    fixed_t z;
    Mobj* th;

    x = mo.x
        + Doom::Fixed {(Doom::randomness().forPlay() - Doom::randomness().forPlay())
                       * 2048};
    y = mo.y;
    z = Doom::Fixed {128} + Doom::randomness().forPlay() * 2 * FRACUNIT;
    th = Doom::spawnMobj(x, y, z, MT_ROCKET);
    th->momz = Doom::Fixed {Doom::randomness().forPlay() * 512};

    Doom::setMobjState(th, S_BRAINEXPLODE1);

    th->tics -= Doom::randomness().forPlay() & 7;
    if (th->tics < 1)
        th->tics = 1;
}

void brainDie(Mobj&)
{
    Doom::exitLevel();
}

void brainSpit(Mobj& mo)
{
    Mobj* targ;
    Mobj* newmobj;

    auto& ai = enemyAI();

    ai.brainSpitEasy ^= 1;
    if (gameSession().gameskill <= sk_easy && (!ai.brainSpitEasy))
        return;

    // shoot a cube at current target
    targ = ai.braintargets[ai.braintargeton];
    ai.braintargeton = (ai.braintargeton + 1) % ai.numbraintargets;

    // spawn brain missile
    newmobj = Doom::spawnMissile(&mo, targ, MT_SPAWNSHOT);
    newmobj->target = targ;
    // Vanilla divides the raw values as plain integers here - the result is a tic
    // count, not a length. A fixed-point divide would scale it by 65536.
    newmobj->reactiontime =
        ((targ->y - mo.y).raw / newmobj->momy.raw) / newmobj->state->tics;

    Doom::startSound(0, sfx_bospit);
}

// travelling cube sound
void spawnSound(Mobj& mo)
{
    Doom::startSound(&mo, sfx_boscub);
    spawnFly(mo);
}

void spawnFly(Mobj& mo)
{
    Mobj* newmobj;
    Mobj* fog;
    Mobj* targ;
    int r;
    MobjType type;

    if (--mo.reactiontime)
        return; // still flying

    targ = mo.target;

    // First spawn teleport fog.
    fog = Doom::spawnMobj(targ->x, targ->y, targ->z, MT_SPAWNFIRE);
    Doom::startSound(fog, sfx_telept);

    // Randomly select monster to spawn.
    r = Doom::randomness().forPlay();

    // Probability distribution (kind of :),
    // decreasing likelihood.
    if (r < 50)
        type = MT_TROOP;
    else if (r < 90)
        type = MT_SERGEANT;
    else if (r < 120)
        type = MT_SHADOWS;
    else if (r < 130)
        type = MT_PAIN;
    else if (r < 160)
        type = MT_HEAD;
    else if (r < 162)
        type = MT_VILE;
    else if (r < 172)
        type = MT_UNDEAD;
    else if (r < 192)
        type = MT_BABY;
    else if (r < 222)
        type = MT_FATSO;
    else if (r < 246)
        type = MT_KNIGHT;
    else
        type = MT_BRUISER;

    newmobj = Doom::spawnMobj(targ->x, targ->y, targ->z, type);
    if (lookForPlayers(*newmobj, true))
        Doom::setMobjState(newmobj, static_cast<StateNum>(newmobj->info->seestate));

    // telefrag anything in this spot
    Doom::teleportMove(newmobj, newmobj->x, newmobj->y);

    // remove self (i.e., cube).
    Doom::removeMobj(&mo);
}

void playerScream(Mobj& mo)
{
    // Default death sound.
    int sound = sfx_pldeth;

    if ((gameVersion().gamemode == commercial) && (mo.health < -50))
    {
        // IF THE PLAYER DIES
        // LESS THAN -50% WITHOUT GIBBING
        sound = sfx_pdiehi;
    }

    Doom::startSound(&mo, sound);
}
} // namespace Doom
