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
// vanilla A_*/P_NoiseAlert names as shims (info.cpp's state table references the A_*
// by address). The AI scratch is file-local; soundtarget stays global (p_saveg
// archives it), and the thinker-identity comparison keeps the global P_MobjThinker.
//
//-----------------------------------------------------------------------------

#include "../doom_config.h"

#include "../doomdef.h"
#include "../doomstat.h"
#include "../g_game.h"
#include "../i_system.h"
#include "../m_random.h"
#include "../p_local.h"
#include "../r_state.h"
#include "../s_sound.h"
#include "../sounds.h"

#include "Enemy.h"
#include "EnemyAI.h"

#define MAXSPECIALCROSS 8
#define FATSPREAD (ANG90 / 8)
#define SKULLSPEED (20 * FRACUNIT)

// A_CloseShotgun2 calls the weapon refire; soundtarget and spechit/numspechit are
// globals the AI shares (p_saveg archives soundtarget; spechit is Clip's).
void A_ReFire(player_t* player, pspdef_t* psp);
extern mobj_t* soundtarget;
extern line_t** spechit;
extern int& numspechit;

namespace Doom
{
// P_NewChaseDir movement LUTs and the transient targets the AI threads through its
// state actions (the vile's corpse, the fat/brain spit targets). All file-local;
// soundtarget alone is global, because p_saveg archives it.
enum dirtype_t
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

dirtype_t opposite[] = {DI_WEST,
                        DI_SOUTHWEST,
                        DI_SOUTH,
                        DI_SOUTHEAST,
                        DI_EAST,
                        DI_NORTHEAST,
                        DI_NORTH,
                        DI_NORTHWEST,
                        DI_NODIR};

dirtype_t diags[] = {DI_NORTHWEST, DI_NORTHEAST, DI_SOUTHWEST, DI_SOUTHEAST};

fixed_t xspeed[8] = {FRACUNIT, 47000, 0, -47000, -FRACUNIT, -47000, 0, 47000};
fixed_t yspeed[8] = {0, 47000, FRACUNIT, 47000, 0, -47000, -FRACUNIT, -47000};
int TRACEANGLE = 0xc000000;
// The monster-AI scratch now lives on the Engine (Sim/EnemyAI.h, moved by the file-scope-statics
// sweep - REFACTOR.md, Step 5). The vanilla names are references onto that member; read by no other
// file. (The const direction/speed tables above stay file-local.)
static mobj_t*& corpsehit = enemyAI().corpsehit;
static mobj_t*& vileobj = enemyAI().vileobj;
static fixed_t& viletryx = enemyAI().viletryx;
static fixed_t& viletryy = enemyAI().viletryy;
static mobj_t* (&braintargets)[32] = enemyAI().braintargets;
static int& numbraintargets = enemyAI().numbraintargets;
static int& braintargeton = enemyAI().braintargeton;

// Forward declarations so the file's own call order needs no rearranging.
void recursiveSound(sector_t* sec, int soundblocks);
void noiseAlert(mobj_t* target, mobj_t* emmiter);
doom_boolean checkMeleeRange(mobj_t* actor);
doom_boolean checkMissileRange(mobj_t* actor);
doom_boolean move(mobj_t* actor);
doom_boolean tryWalk(mobj_t* actor);
void newChaseDir(mobj_t* actor);
doom_boolean lookForPlayers(mobj_t* actor, doom_boolean allaround);
void keenDie(mobj_t* mo);
void look(mobj_t* actor);
void chase(mobj_t* actor);
void faceTarget(mobj_t* actor);
void posAttack(mobj_t* actor);
void sPosAttack(mobj_t* actor);
void cPosAttack(mobj_t* actor);
void cPosRefire(mobj_t* actor);
void spidRefire(mobj_t* actor);
void bspiAttack(mobj_t* actor);
void troopAttack(mobj_t* actor);
void sargAttack(mobj_t* actor);
void headAttack(mobj_t* actor);
void cyberAttack(mobj_t* actor);
void bruisAttack(mobj_t* actor);
void skelMissile(mobj_t* actor);
void tracer(mobj_t* actor);
void skelWhoosh(mobj_t* actor);
void skelFist(mobj_t* actor);
doom_boolean vileCheck(mobj_t* thing);
void vileChase(mobj_t* actor);
void vileStart(mobj_t* actor);
void startFire(mobj_t* actor);
void fireCrackle(mobj_t* actor);
void fire(mobj_t* actor);
void vileTarget(mobj_t* actor);
void vileAttack(mobj_t* actor);
void fatRaise(mobj_t* actor);
void fatAttack1(mobj_t* actor);
void fatAttack2(mobj_t* actor);
void fatAttack3(mobj_t* actor);
void skullAttack(mobj_t* actor);
void painShootSkull(mobj_t* actor, angle_t angle);
void painAttack(mobj_t* actor);
void painDie(mobj_t* actor);
void scream(mobj_t* actor);
void xScream(mobj_t* actor);
void pain(mobj_t* actor);
void fall(mobj_t* actor);
void explode(mobj_t* thingy);
void bossDeath(mobj_t* mo);
void hoof(mobj_t* mo);
void metal(mobj_t* mo);
void babyMetal(mobj_t* mo);
void openShotgun2(player_t* player, pspdef_t* psp);
void loadShotgun2(player_t* player, pspdef_t* psp);
void closeShotgun2(player_t* player, pspdef_t* psp);
void brainAwake(mobj_t* mo);
void brainPain(mobj_t* mo);
void brainScream(mobj_t* mo);
void brainExplode(mobj_t* mo);
void brainDie(mobj_t* mo);
void brainSpit(mobj_t* mo);
void spawnSound(mobj_t* mo);
void spawnFly(mobj_t* mo);
void playerScream(mobj_t* mo);

void recursiveSound(sector_t* sec, int soundblocks)
{
    int i;
    line_t* check;
    sector_t* other;

    // wake up all monsters in this sector
    if (sec->validcount == validcount && sec->soundtraversed <= soundblocks + 1)
    {
        return; // already flooded
    }

    sec->validcount = validcount;
    sec->soundtraversed = soundblocks + 1;
    sec->soundtarget = soundtarget;

    for (i = 0; i < sec->linecount; i++)
    {
        check = sec->lines[i];
        if (!(check->flags & ML_TWOSIDED))
            continue;

        P_LineOpening(check);

        if (openrange <= 0)
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
void noiseAlert(mobj_t* target, mobj_t* emmiter)
{
    soundtarget = target;
    validcount++;
    recursiveSound(emmiter->subsector->sector, 0);
}

//
// checkMeleeRange
//
doom_boolean checkMeleeRange(mobj_t* actor)
{
    mobj_t* pl;
    fixed_t dist;

    if (!actor->target)
        return false;

    pl = actor->target;
    dist = P_AproxDistance(pl->x - actor->x, pl->y - actor->y);

    if (dist >= MELEERANGE - 20 * FRACUNIT + pl->info->radius)
        return false;

    if (!P_CheckSight(actor, actor->target))
        return false;

    return true;
}

//
// checkMissileRange
//
doom_boolean checkMissileRange(mobj_t* actor)
{
    fixed_t dist;

    if (!P_CheckSight(actor, actor->target))
        return false;

    if (actor->flags & MF_JUSTHIT)
    {
        // the target just hit the enemy,
        // so fight back!
        actor->flags &= ~MF_JUSTHIT;
        return true;
    }

    if (actor->reactiontime)
        return false; // do not attack yet

    // OPTIMIZE: get this from a global checksight
    dist = P_AproxDistance(actor->x - actor->target->x, actor->y - actor->target->y)
           - 64 * FRACUNIT;

    if (!actor->info->meleestate)
        dist -= 128 * FRACUNIT; // no melee attack, so fire more

    dist >>= 16;

    if (actor->type == MT_VILE)
    {
        if (dist > 14 * 64)
            return false; // too far away
    }

    if (actor->type == MT_UNDEAD)
    {
        if (dist < 196)
            return false; // close for fist attack
        dist >>= 1;
    }

    if (actor->type == MT_CYBORG || actor->type == MT_SPIDER
        || actor->type == MT_SKULL)
    {
        dist >>= 1;
    }

    if (dist > 200)
        dist = 200;

    if (actor->type == MT_CYBORG && dist > 160)
        dist = 160;

    if (P_Random() < dist)
        return false;

    return true;
}

//
// move
// Move in the current direction,
// returns false if the move is blocked.
//
doom_boolean move(mobj_t* actor)
{
    fixed_t tryx;
    fixed_t tryy;

    line_t* ld;

    // warning: 'catch', 'throw', and 'try'
    // are all C++ reserved words
    doom_boolean try_ok;
    doom_boolean good;

    if (actor->movedir == DI_NODIR)
        return false;

    if ((unsigned) actor->movedir >= 8)
        I_Error("Error: Weird actor->movedir!");

    tryx = actor->x + actor->info->speed * xspeed[actor->movedir];
    tryy = actor->y + actor->info->speed * yspeed[actor->movedir];

    try_ok = P_TryMove(actor, tryx, tryy);

    if (!try_ok)
    {
        // open any specials
        if (actor->flags & MF_FLOAT && floatok)
        {
            // must adjust height
            if (actor->z < tmfloorz)
                actor->z += FLOATSPEED;
            else
                actor->z -= FLOATSPEED;

            actor->flags |= MF_INFLOAT;
            return true;
        }

        if (!numspechit)
            return false;

        actor->movedir = DI_NODIR;
        good = false;
        while (numspechit--)
        {
            ld = spechit[numspechit];
            // if the special is not a door
            // that can be opened,
            // return false
            if (P_UseSpecialLine(actor, ld, 0))
                good = true;
        }
        return good;
    }
    else
    {
        actor->flags &= ~MF_INFLOAT;
    }

    if (!(actor->flags & MF_FLOAT))
        actor->z = actor->floorz;

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
doom_boolean tryWalk(mobj_t* actor)
{
    if (!move(actor))
    {
        return false;
    }

    actor->movecount = P_Random() & 15;
    return true;
}

void newChaseDir(mobj_t* actor)
{
    fixed_t deltax;
    fixed_t deltay;

    dirtype_t d[3];

    int tdir;
    dirtype_t olddir;

    dirtype_t turnaround;

    if (!actor->target)
        I_Error("Error: newChaseDir: called with no target");

    olddir = (dirtype_t) (actor->movedir);
    turnaround = opposite[olddir];

    deltax = actor->target->x - actor->x;
    deltay = actor->target->y - actor->y;

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
        actor->movedir = diags[((deltay < 0) << 1) + (deltax > 0)];
        if (actor->movedir != turnaround && tryWalk(actor))
            return;
    }

    // try other directions
    if (P_Random() > 200 || doom_abs(deltay) > doom_abs(deltax))
    {
        tdir = d[1];
        d[1] = d[2];
        d[2] = (dirtype_t) (tdir);
    }

    if (d[1] == turnaround)
        d[1] = DI_NODIR;
    if (d[2] == turnaround)
        d[2] = DI_NODIR;

    if (d[1] != DI_NODIR)
    {
        actor->movedir = d[1];
        if (tryWalk(actor))
        {
            // either moved forward or attacked
            return;
        }
    }

    if (d[2] != DI_NODIR)
    {
        actor->movedir = d[2];

        if (tryWalk(actor))
            return;
    }

    // there is no direct path to the player,
    // so pick another direction.
    if (olddir != DI_NODIR)
    {
        actor->movedir = olddir;

        if (tryWalk(actor))
            return;
    }

    // randomly determine direction of search
    if (P_Random() & 1)
    {
        for (tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++)
        {
            if (tdir != turnaround)
            {
                actor->movedir = tdir;

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
                actor->movedir = tdir;

                if (tryWalk(actor))
                    return;
            }
        }
    }

    if (turnaround != DI_NODIR)
    {
        actor->movedir = turnaround;
        if (tryWalk(actor))
            return;
    }

    actor->movedir = DI_NODIR; // can not move
}

//
// lookForPlayers
// If allaround is false, only look 180 degrees in front.
// Returns true if a player is targeted.
//
doom_boolean lookForPlayers(mobj_t* actor, doom_boolean allaround)
{
    int c;
    int stop;
    player_t* player;
    angle_t an;
    fixed_t dist;

    c = 0;
    stop = (actor->lastlook - 1) & 3;

    for (;; actor->lastlook = (actor->lastlook + 1) & 3)
    {
        if (!playeringame[actor->lastlook])
            continue;

        if (c++ == 2 || actor->lastlook == stop)
        {
            // done looking
            return false;
        }

        player = &players[actor->lastlook];

        if (player->health <= 0)
            continue; // dead

        if (!P_CheckSight(actor, player->mo))
            continue; // out of sight

        if (!allaround)
        {
            an = R_PointToAngle2(actor->x, actor->y, player->mo->x, player->mo->y)
                 - actor->angle;

            if (an > ANG90 && an < ANG270)
            {
                dist = P_AproxDistance(player->mo->x - actor->x,
                                       player->mo->y - actor->y);
                // if real close, react anyway
                if (dist > MELEERANGE)
                    continue; // behind back
            }
        }

        actor->target = player->mo;
        return true;
    }

    return false;
}

//
// keenDie
// DOOM II special, map 32.
// Uses special tag 666.
//
void keenDie(mobj_t* mo)
{
    thinker_t* th;
    mobj_t* mo2;
    line_t junk;

    fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for (th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if (th->function.acp1 != (actionf_p1) P_MobjThinker)
            continue;

        mo2 = (mobj_t*) th;
        if (mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    junk.tag = 666;
    EV_DoDoor(&junk, door_open);
}

//
// ACTION ROUTINES
//

//
// look
// Stay in state until a player is sighted.
//
void look(mobj_t* actor)
{
    mobj_t* targ;

    actor->threshold = 0; // any shot will wake up
    targ = actor->subsector->sector->soundtarget;

    if (targ && (targ->flags & MF_SHOOTABLE))
    {
        actor->target = targ;

        if (actor->flags & MF_AMBUSH)
        {
            if (P_CheckSight(actor, actor->target))
                goto seeyou;
        }
        else
            goto seeyou;
    }

    if (!lookForPlayers(actor, false))
        return;

    // go into chase state
seeyou:
    if (actor->info->seesound)
    {
        int sound;

        switch (actor->info->seesound)
        {
            case sfx_posit1:
            case sfx_posit2:
            case sfx_posit3:
                sound = sfx_posit1 + P_Random() % 3;
                break;

            case sfx_bgsit1:
            case sfx_bgsit2:
                sound = sfx_bgsit1 + P_Random() % 2;
                break;

            default:
                sound = actor->info->seesound;
                break;
        }

        if (actor->type == MT_SPIDER || actor->type == MT_CYBORG)
        {
            // full volume
            S_StartSound(0, sound);
        }
        else
            S_StartSound(actor, sound);
    }

    P_SetMobjState(actor, (statenum_t) (actor->info->seestate));
}

//
// chase
// Actor has a melee attack,
// so it tries to close as fast as possible
//
void chase(mobj_t* actor)
{
    int delta;

    if (actor->reactiontime)
        actor->reactiontime--;

    // modify target threshold
    if (actor->threshold)
    {
        if (!actor->target || actor->target->health <= 0)
        {
            actor->threshold = 0;
        }
        else
            actor->threshold--;
    }

    // turn towards movement direction if not there yet
    if (actor->movedir < 8)
    {
        actor->angle &= (7 << 29);
        delta = actor->angle - (actor->movedir << 29);

        if (delta > 0)
            actor->angle -= ANG90 / 2;
        else if (delta < 0)
            actor->angle += ANG90 / 2;
    }

    if (!actor->target || !(actor->target->flags & MF_SHOOTABLE))
    {
        // look for a new target
        if (lookForPlayers(actor, true))
            return; // got a new target

        P_SetMobjState(actor, (statenum_t) (actor->info->spawnstate));
        return;
    }

    // do not attack twice in a row
    if (actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if (gameskill != sk_nightmare && !fastparm)
            newChaseDir(actor);
        return;
    }

    // check for melee attack
    if (actor->info->meleestate && checkMeleeRange(actor))
    {
        if (actor->info->attacksound)
            S_StartSound(actor, actor->info->attacksound);

        P_SetMobjState(actor, (statenum_t) (actor->info->meleestate));
        return;
    }

    // check for missile attack
    if (actor->info->missilestate)
    {
        if (gameskill < sk_nightmare && !fastparm && actor->movecount)
        {
            goto nomissile;
        }

        if (!checkMissileRange(actor))
            goto nomissile;

        P_SetMobjState(actor, (statenum_t) (actor->info->missilestate));
        actor->flags |= MF_JUSTATTACKED;
        return;
    }

    // ?
nomissile:
    // possibly choose another target
    if (netgame && !actor->threshold && !P_CheckSight(actor, actor->target))
    {
        if (lookForPlayers(actor, true))
            return; // got a new target
    }

    // chase towards player
    if (--actor->movecount < 0 || !move(actor))
    {
        newChaseDir(actor);
    }

    // make active sound
    if (actor->info->activesound && P_Random() < 3)
    {
        S_StartSound(actor, actor->info->activesound);
    }
}

//
// faceTarget
//
void faceTarget(mobj_t* actor)
{
    if (!actor->target)
        return;

    actor->flags &= ~MF_AMBUSH;

    actor->angle =
        R_PointToAngle2(actor->x, actor->y, actor->target->x, actor->target->y);

    if (actor->target->flags & MF_SHADOW)
        actor->angle += (P_Random() - P_Random()) << 21;
}

//
// posAttack
//
void posAttack(mobj_t* actor)
{
    int angle;
    int damage;
    int slope;

    if (!actor->target)
        return;

    faceTarget(actor);
    angle = actor->angle;
    slope = P_AimLineAttack(actor, angle, MISSILERANGE);

    S_StartSound(actor, sfx_pistol);
    angle += (P_Random() - P_Random()) << 20;
    damage = ((P_Random() % 5) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void sPosAttack(mobj_t* actor)
{
    int i;
    int angle;
    int bangle;
    int damage;
    int slope;

    if (!actor->target)
        return;

    S_StartSound(actor, sfx_shotgn);
    faceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    for (i = 0; i < 3; i++)
    {
        angle = bangle + ((P_Random() - P_Random()) << 20);
        damage = ((P_Random() % 5) + 1) * 3;
        P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
    }
}

void cPosAttack(mobj_t* actor)
{
    int angle;
    int bangle;
    int damage;
    int slope;

    if (!actor->target)
        return;

    S_StartSound(actor, sfx_shotgn);
    faceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random() - P_Random()) << 20);
    damage = ((P_Random() % 5) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void cPosRefire(mobj_t* actor)
{
    // keep firing unless target got out of sight
    faceTarget(actor);

    if (P_Random() < 40)
        return;

    if (!actor->target || actor->target->health <= 0
        || !P_CheckSight(actor, actor->target))
    {
        P_SetMobjState(actor, (statenum_t) (actor->info->seestate));
    }
}

void spidRefire(mobj_t* actor)
{
    // keep firing unless target got out of sight
    faceTarget(actor);

    if (P_Random() < 10)
        return;

    if (!actor->target || actor->target->health <= 0
        || !P_CheckSight(actor, actor->target))
    {
        P_SetMobjState(actor, (statenum_t) (actor->info->seestate));
    }
}

void bspiAttack(mobj_t* actor)
{
    if (!actor->target)
        return;

    faceTarget(actor);

    // launch a missile
    P_SpawnMissile(actor, actor->target, MT_ARACHPLAZ);
}

//
// troopAttack
//
void troopAttack(mobj_t* actor)
{
    int damage;

    if (!actor->target)
        return;

    faceTarget(actor);
    if (checkMeleeRange(actor))
    {
        S_StartSound(actor, sfx_claw);
        damage = (P_Random() % 8 + 1) * 3;
        P_DamageMobj(actor->target, actor, actor, damage);
        return;
    }

    // launch a missile
    P_SpawnMissile(actor, actor->target, MT_TROOPSHOT);
}

void sargAttack(mobj_t* actor)
{
    int damage;

    if (!actor->target)
        return;

    faceTarget(actor);
    if (checkMeleeRange(actor))
    {
        damage = ((P_Random() % 10) + 1) * 4;
        P_DamageMobj(actor->target, actor, actor, damage);
    }
}

void headAttack(mobj_t* actor)
{
    int damage;

    if (!actor->target)
        return;

    faceTarget(actor);
    if (checkMeleeRange(actor))
    {
        damage = (P_Random() % 6 + 1) * 10;
        P_DamageMobj(actor->target, actor, actor, damage);
        return;
    }

    // launch a missile
    P_SpawnMissile(actor, actor->target, MT_HEADSHOT);
}

void cyberAttack(mobj_t* actor)
{
    if (!actor->target)
        return;

    faceTarget(actor);
    P_SpawnMissile(actor, actor->target, MT_ROCKET);
}

void bruisAttack(mobj_t* actor)
{
    int damage;

    if (!actor->target)
        return;

    if (checkMeleeRange(actor))
    {
        S_StartSound(actor, sfx_claw);
        damage = (P_Random() % 8 + 1) * 10;
        P_DamageMobj(actor->target, actor, actor, damage);
        return;
    }

    // launch a missile
    P_SpawnMissile(actor, actor->target, MT_BRUISERSHOT);
}

//
// skelMissile
//
void skelMissile(mobj_t* actor)
{
    mobj_t* mo;

    if (!actor->target)
        return;

    faceTarget(actor);
    actor->z += 16 * FRACUNIT; // so missile spawns higher
    mo = P_SpawnMissile(actor, actor->target, MT_TRACER);
    actor->z -= 16 * FRACUNIT; // back to normal

    mo->x += mo->momx;
    mo->y += mo->momy;
    mo->tracer = actor->target;
}

void tracer(mobj_t* actor)
{
    angle_t exact;
    fixed_t dist;
    fixed_t slope;
    mobj_t* dest;
    mobj_t* th;

    if (gametic & 3)
        return;

    // spawn a puff of smoke behind the rocket
    P_SpawnPuff(actor->x, actor->y, actor->z);

    th = P_SpawnMobj(
        actor->x - actor->momx, actor->y - actor->momy, actor->z, MT_SMOKE);

    th->momz = FRACUNIT;
    th->tics -= P_Random() & 3;
    if (th->tics < 1)
        th->tics = 1;

    // adjust direction
    dest = actor->tracer;

    if (!dest || dest->health <= 0)
        return;

    // change angle
    exact = R_PointToAngle2(actor->x, actor->y, dest->x, dest->y);

    if (exact != actor->angle)
    {
        if (exact - actor->angle > 0x80000000)
        {
            actor->angle -= TRACEANGLE;
            if (exact - actor->angle < 0x80000000)
                actor->angle = exact;
        }
        else
        {
            actor->angle += TRACEANGLE;
            if (exact - actor->angle > 0x80000000)
                actor->angle = exact;
        }
    }

    exact = actor->angle >> ANGLETOFINESHIFT;
    actor->momx = FixedMul(actor->info->speed, finecosine[exact]);
    actor->momy = FixedMul(actor->info->speed, finesine[exact]);

    // change slope
    dist = P_AproxDistance(dest->x - actor->x, dest->y - actor->y);

    dist = dist / actor->info->speed;

    if (dist < 1)
        dist = 1;
    slope = (dest->z + 40 * FRACUNIT - actor->z) / dist;

    if (slope < actor->momz)
        actor->momz -= FRACUNIT / 8;
    else
        actor->momz += FRACUNIT / 8;
}

void skelWhoosh(mobj_t* actor)
{
    if (!actor->target)
        return;

    faceTarget(actor);
    S_StartSound(actor, sfx_skeswg);
}

void skelFist(mobj_t* actor)
{
    int damage;

    if (!actor->target)
        return;

    faceTarget(actor);

    if (checkMeleeRange(actor))
    {
        damage = ((P_Random() % 10) + 1) * 6;
        S_StartSound(actor, sfx_skepch);
        P_DamageMobj(actor->target, actor, actor, damage);
    }
}

//
// vileCheck
// Detect a corpse that could be raised.
//
doom_boolean vileCheck(mobj_t* thing)
{
    int maxdist;
    doom_boolean check;

    if (!(thing->flags & MF_CORPSE))
        return true; // not a monster

    if (thing->tics != -1)
        return true; // not lying still yet

    if (thing->info->raisestate == S_NULL)
        return true; // monster doesn't have a raise state

    maxdist = thing->info->radius + mobjinfo[MT_VILE].radius;

    if (doom_abs(thing->x - viletryx) > maxdist
        || doom_abs(thing->y - viletryy) > maxdist)
        return true; // not actually touching

    corpsehit = thing;
    corpsehit->momx = corpsehit->momy = 0;
    corpsehit->height <<= 2;
    check = P_CheckPosition(corpsehit, corpsehit->x, corpsehit->y);
    corpsehit->height >>= 2;

    if (!check)
        return true; // doesn't fit here

    return false; // got one, so stop checking
}

//
// vileChase
// Check for ressurecting a body
//
void vileChase(mobj_t* actor)
{
    int xl;
    int xh;
    int yl;
    int yh;

    int bx;
    int by;

    mobjinfo_t* info;
    mobj_t* temp;

    if (actor->movedir != DI_NODIR)
    {
        // check for corpses to raise
        viletryx = actor->x + actor->info->speed * xspeed[actor->movedir];
        viletryy = actor->y + actor->info->speed * yspeed[actor->movedir];

        xl = (viletryx - bmaporgx - MAXRADIUS * 2) >> MAPBLOCKSHIFT;
        xh = (viletryx - bmaporgx + MAXRADIUS * 2) >> MAPBLOCKSHIFT;
        yl = (viletryy - bmaporgy - MAXRADIUS * 2) >> MAPBLOCKSHIFT;
        yh = (viletryy - bmaporgy + MAXRADIUS * 2) >> MAPBLOCKSHIFT;

        vileobj = actor;
        for (bx = xl; bx <= xh; bx++)
        {
            for (by = yl; by <= yh; by++)
            {
                // Call vileCheck to check
                // whether object is a corpse
                // that canbe raised.
                if (!P_BlockThingsIterator(bx, by, vileCheck))
                {
                    // got one!
                    temp = actor->target;
                    actor->target = corpsehit;
                    faceTarget(actor);
                    actor->target = temp;

                    P_SetMobjState(actor, S_VILE_HEAL1);
                    S_StartSound(corpsehit, sfx_slop);
                    info = corpsehit->info;

                    P_SetMobjState(corpsehit, (statenum_t) (info->raisestate));
                    corpsehit->height <<= 2;
                    corpsehit->flags = info->flags;
                    corpsehit->health = info->spawnhealth;
                    corpsehit->target = 0;

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
void vileStart(mobj_t* actor)
{
    S_StartSound(actor, sfx_vilatk);
}

//
// fire
// Keep fire in front of player unless out of sight
//
void startFire(mobj_t* actor)
{
    S_StartSound(actor, sfx_flamst);
    fire(actor);
}

void fireCrackle(mobj_t* actor)
{
    S_StartSound(actor, sfx_flame);
    fire(actor);
}

void fire(mobj_t* actor)
{
    mobj_t* dest;
    unsigned an;

    dest = actor->tracer;
    if (!dest)
        return;

    // don't move it if the vile lost sight
    if (!P_CheckSight(actor->target, dest))
        return;

    an = dest->angle >> ANGLETOFINESHIFT;

    P_UnsetThingPosition(actor);
    actor->x = dest->x + FixedMul(24 * FRACUNIT, finecosine[an]);
    actor->y = dest->y + FixedMul(24 * FRACUNIT, finesine[an]);
    actor->z = dest->z;
    P_SetThingPosition(actor);
}

//
// vileTarget
// Spawn the hellfire
//
void vileTarget(mobj_t* actor)
{
    mobj_t* fog;

    if (!actor->target)
        return;

    faceTarget(actor);

    fog = P_SpawnMobj(actor->target->x, actor->target->x, actor->target->z, MT_FIRE);

    actor->tracer = fog;
    fog->target = actor;
    fog->tracer = actor->target;
    fire(fog);
}

//
// vileAttack
//
void vileAttack(mobj_t* actor)
{
    mobj_t* fire;
    int an;

    if (!actor->target)
        return;

    faceTarget(actor);

    if (!P_CheckSight(actor, actor->target))
        return;

    S_StartSound(actor, sfx_barexp);
    P_DamageMobj(actor->target, actor, actor, 20);
    actor->target->momz = 1000 * FRACUNIT / actor->target->info->mass;

    an = actor->angle >> ANGLETOFINESHIFT;

    fire = actor->tracer;

    if (!fire)
        return;

    // move the fire between the vile and the player
    fire->x = actor->target->x - FixedMul(24 * FRACUNIT, finecosine[an]);
    fire->y = actor->target->y - FixedMul(24 * FRACUNIT, finesine[an]);
    P_RadiusAttack(fire, actor, 70);
}

//
// Mancubus attack,
// firing three missiles (bruisers)
// in three different directions?
// Doesn't look like it.
//
void fatRaise(mobj_t* actor)
{
    faceTarget(actor);
    S_StartSound(actor, sfx_manatk);
}

void fatAttack1(mobj_t* actor)
{
    mobj_t* mo;
    int an;

    faceTarget(actor);
    // Change direction  to ...
    actor->angle += FATSPREAD;
    P_SpawnMissile(actor, actor->target, MT_FATSHOT);

    mo = P_SpawnMissile(actor, actor->target, MT_FATSHOT);
    mo->angle += FATSPREAD;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul(mo->info->speed, finecosine[an]);
    mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

void fatAttack2(mobj_t* actor)
{
    mobj_t* mo;
    int an;

    faceTarget(actor);
    // Now here choose opposite deviation.
    actor->angle -= FATSPREAD;
    P_SpawnMissile(actor, actor->target, MT_FATSHOT);

    mo = P_SpawnMissile(actor, actor->target, MT_FATSHOT);
    mo->angle -= FATSPREAD * 2;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul(mo->info->speed, finecosine[an]);
    mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

void fatAttack3(mobj_t* actor)
{
    mobj_t* mo;
    int an;

    faceTarget(actor);

    mo = P_SpawnMissile(actor, actor->target, MT_FATSHOT);
    mo->angle -= FATSPREAD / 2;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul(mo->info->speed, finecosine[an]);
    mo->momy = FixedMul(mo->info->speed, finesine[an]);

    mo = P_SpawnMissile(actor, actor->target, MT_FATSHOT);
    mo->angle += FATSPREAD / 2;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul(mo->info->speed, finecosine[an]);
    mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// SkullAttack
// Fly at the player like a missile.
//
void skullAttack(mobj_t* actor)
{
    mobj_t* dest;
    angle_t an;
    int dist;

    if (!actor->target)
        return;

    dest = actor->target;
    actor->flags |= MF_SKULLFLY;

    S_StartSound(actor, actor->info->attacksound);
    faceTarget(actor);
    an = actor->angle >> ANGLETOFINESHIFT;
    actor->momx = FixedMul(SKULLSPEED, finecosine[an]);
    actor->momy = FixedMul(SKULLSPEED, finesine[an]);
    dist = P_AproxDistance(dest->x - actor->x, dest->y - actor->y);
    dist = dist / SKULLSPEED;

    if (dist < 1)
        dist = 1;
    actor->momz = (dest->z + (dest->height >> 1) - actor->z) / dist;
}

//
// painShootSkull
// Spawn a lost soul and launch it at the target
//
void painShootSkull(mobj_t* actor, angle_t angle)
{
    fixed_t x;
    fixed_t y;
    fixed_t z;

    mobj_t* newmobj;
    angle_t an;
    int prestep;
    int count;
    thinker_t* currentthinker;

    // count total number of skull currently on the level
    count = 0;

    currentthinker = thinkercap.next;
    while (currentthinker != &thinkercap)
    {
        if ((currentthinker->function.acp1 == (actionf_p1) P_MobjThinker)
            && ((mobj_t*) currentthinker)->type == MT_SKULL)
            count++;
        currentthinker = currentthinker->next;
    }

    // if there are allready 20 skulls on the level,
    // don't spit another one
    if (count > 20)
        return;

    // okay, there's playe for another one
    an = angle >> ANGLETOFINESHIFT;

    prestep =
        4 * FRACUNIT + 3 * (actor->info->radius + mobjinfo[MT_SKULL].radius) / 2;

    x = actor->x + FixedMul(prestep, finecosine[an]);
    y = actor->y + FixedMul(prestep, finesine[an]);
    z = actor->z + 8 * FRACUNIT;

    newmobj = P_SpawnMobj(x, y, z, MT_SKULL);

    // Check for movements.
    if (!P_TryMove(newmobj, newmobj->x, newmobj->y))
    {
        // kill it immediately
        P_DamageMobj(newmobj, actor, actor, 10000);
        return;
    }

    newmobj->target = actor->target;
    skullAttack(newmobj);
}

//
// painAttack
// Spawn a lost soul and launch it at the target
//
void painAttack(mobj_t* actor)
{
    if (!actor->target)
        return;

    faceTarget(actor);
    painShootSkull(actor, actor->angle);
}

void painDie(mobj_t* actor)
{
    fall(actor);
    painShootSkull(actor, actor->angle + ANG90);
    painShootSkull(actor, actor->angle + ANG180);
    painShootSkull(actor, actor->angle + ANG270);
}

void scream(mobj_t* actor)
{
    int sound;

    switch (actor->info->deathsound)
    {
        case 0:
            return;

        case sfx_podth1:
        case sfx_podth2:
        case sfx_podth3:
            sound = sfx_podth1 + P_Random() % 3;
            break;

        case sfx_bgdth1:
        case sfx_bgdth2:
            sound = sfx_bgdth1 + P_Random() % 2;
            break;

        default:
            sound = actor->info->deathsound;
            break;
    }

    // Check for bosses.
    if (actor->type == MT_SPIDER || actor->type == MT_CYBORG)
    {
        // full volume
        S_StartSound(0, sound);
    }
    else
        S_StartSound(actor, sound);
}

void xScream(mobj_t* actor)
{
    S_StartSound(actor, sfx_slop);
}

void pain(mobj_t* actor)
{
    if (actor->info->painsound)
        S_StartSound(actor, actor->info->painsound);
}

void fall(mobj_t* actor)
{
    // actor is on ground, it can be walked over
    actor->flags &= ~MF_SOLID;

    // So change this if corpse objects
    // are meant to be obstacles.
}

//
// explode
//
void explode(mobj_t* thingy)
{
    P_RadiusAttack(thingy, thingy->target, 128);
}

//
// bossDeath
// Possibly trigger special effects
// if on first boss level
//
void bossDeath(mobj_t* mo)
{
    thinker_t* th;
    mobj_t* mo2;
    line_t junk;
    int i;

    if (gamemode == commercial)
    {
        if (gamemap != 7)
            return;

        if ((mo->type != MT_FATSO) && (mo->type != MT_BABY))
            return;
    }
    else
    {
        switch (gameepisode)
        {
            case 1:
                if (gamemap != 8)
                    return;

                if (mo->type != MT_BRUISER)
                    return;
                break;

            case 2:
                if (gamemap != 8)
                    return;

                if (mo->type != MT_CYBORG)
                    return;
                break;

            case 3:
                if (gamemap != 8)
                    return;

                if (mo->type != MT_SPIDER)
                    return;

                break;

            case 4:
                switch (gamemap)
                {
                    case 6:
                        if (mo->type != MT_CYBORG)
                            return;
                        break;

                    case 8:
                        if (mo->type != MT_SPIDER)
                            return;
                        break;

                    default:
                        return;
                        break;
                }
                break;

            default:
                if (gamemap != 8)
                    return;
                break;
        }
    }

    // make sure there is a player alive for victory
    for (i = 0; i < MAXPLAYERS; i++)
        if (playeringame[i] && players[i].health > 0)
            break;

    if (i == MAXPLAYERS)
        return; // no one left alive, so do not end game

    // scan the remaining thinkers to see
    // if all bosses are dead
    for (th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if (th->function.acp1 != (actionf_p1) P_MobjThinker)
            continue;

        mo2 = (mobj_t*) th;
        if (mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other boss not dead
            return;
        }
    }

    // victory!
    if (gamemode == commercial)
    {
        if (gamemap == 7)
        {
            if (mo->type == MT_FATSO)
            {
                junk.tag = 666;
                EV_DoFloor(&junk, lowerFloorToLowest);
                return;
            }

            if (mo->type == MT_BABY)
            {
                junk.tag = 667;
                EV_DoFloor(&junk, raiseToTexture);
                return;
            }
        }
    }
    else
    {
        switch (gameepisode)
        {
            case 1:
                junk.tag = 666;
                EV_DoFloor(&junk, lowerFloorToLowest);
                return;
                break;

            case 4:
                switch (gamemap)
                {
                    case 6:
                        junk.tag = 666;
                        EV_DoDoor(&junk, blazeOpen);
                        return;
                        break;

                    case 8:
                        junk.tag = 666;
                        EV_DoFloor(&junk, lowerFloorToLowest);
                        return;
                        break;
                }
        }
    }

    G_ExitLevel();
}

void hoof(mobj_t* mo)
{
    S_StartSound(mo, sfx_hoof);
    chase(mo);
}

void metal(mobj_t* mo)
{
    S_StartSound(mo, sfx_metal);
    chase(mo);
}

void babyMetal(mobj_t* mo)
{
    S_StartSound(mo, sfx_bspwlk);
    chase(mo);
}

void openShotgun2(player_t* player, pspdef_t*)
{
    S_StartSound(player->mo, sfx_dbopn);
}

void loadShotgun2(player_t* player, pspdef_t*)
{
    S_StartSound(player->mo, sfx_dbload);
}

void closeShotgun2(player_t* player, pspdef_t* psp)
{
    S_StartSound(player->mo, sfx_dbcls);
    A_ReFire(player, psp);
}

void brainAwake(mobj_t*)
{
    thinker_t* thinker;
    mobj_t* m;

    // find all the target spots
    numbraintargets = 0;
    braintargeton = 0;

    thinker = thinkercap.next;
    for (thinker = thinkercap.next; thinker != &thinkercap; thinker = thinker->next)
    {
        if (thinker->function.acp1 != (actionf_p1) P_MobjThinker)
            continue; // not a mobj

        m = (mobj_t*) thinker;

        if (m->type == MT_BOSSTARGET)
        {
            braintargets[numbraintargets] = m;
            numbraintargets++;
        }
    }

    S_StartSound(0, sfx_bossit);
}

void brainPain(mobj_t*)
{
    S_StartSound(0, sfx_bospn);
}

void brainScream(mobj_t* mo)
{
    int x;
    int y;
    int z;
    mobj_t* th;

    for (x = mo->x - 196 * FRACUNIT; x < mo->x + 320 * FRACUNIT; x += FRACUNIT * 8)
    {
        y = mo->y - 320 * FRACUNIT;
        z = 128 + P_Random() * 2 * FRACUNIT;
        th = P_SpawnMobj(x, y, z, MT_ROCKET);
        th->momz = P_Random() * 512;

        P_SetMobjState(th, S_BRAINEXPLODE1);

        th->tics -= P_Random() & 7;
        if (th->tics < 1)
            th->tics = 1;
    }

    S_StartSound(0, sfx_bosdth);
}

void brainExplode(mobj_t* mo)
{
    int x;
    int y;
    int z;
    mobj_t* th;

    x = mo->x + (P_Random() - P_Random()) * 2048;
    y = mo->y;
    z = 128 + P_Random() * 2 * FRACUNIT;
    th = P_SpawnMobj(x, y, z, MT_ROCKET);
    th->momz = P_Random() * 512;

    P_SetMobjState(th, S_BRAINEXPLODE1);

    th->tics -= P_Random() & 7;
    if (th->tics < 1)
        th->tics = 1;
}

void brainDie(mobj_t*)
{
    G_ExitLevel();
}

void brainSpit(mobj_t* mo)
{
    mobj_t* targ;
    mobj_t* newmobj;

    static int easy = 0;

    easy ^= 1;
    if (gameskill <= sk_easy && (!easy))
        return;

    // shoot a cube at current target
    targ = braintargets[braintargeton];
    braintargeton = (braintargeton + 1) % numbraintargets;

    // spawn brain missile
    newmobj = P_SpawnMissile(mo, targ, MT_SPAWNSHOT);
    newmobj->target = targ;
    newmobj->reactiontime =
        ((targ->y - mo->y) / newmobj->momy) / newmobj->state->tics;

    S_StartSound(0, sfx_bospit);
}

// travelling cube sound
void spawnSound(mobj_t* mo)
{
    S_StartSound(mo, sfx_boscub);
    spawnFly(mo);
}

void spawnFly(mobj_t* mo)
{
    mobj_t* newmobj;
    mobj_t* fog;
    mobj_t* targ;
    int r;
    mobjtype_t type;

    if (--mo->reactiontime)
        return; // still flying

    targ = mo->target;

    // First spawn teleport fog.
    fog = P_SpawnMobj(targ->x, targ->y, targ->z, MT_SPAWNFIRE);
    S_StartSound(fog, sfx_telept);

    // Randomly select monster to spawn.
    r = P_Random();

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

    newmobj = P_SpawnMobj(targ->x, targ->y, targ->z, type);
    if (lookForPlayers(newmobj, true))
        P_SetMobjState(newmobj, (statenum_t) (newmobj->info->seestate));

    // telefrag anything in this spot
    P_TeleportMove(newmobj, newmobj->x, newmobj->y);

    // remove self (i.e., cube).
    P_RemoveMobj(mo);
}

void playerScream(mobj_t* mo)
{
    // Default death sound.
    int sound = sfx_pldeth;

    if ((gamemode == commercial) && (mo->health < -50))
    {
        // IF THE PLAYER DIES
        // LESS THAN -50% WITHOUT GIBBING
        sound = sfx_pdiehi;
    }

    S_StartSound(mo, sound);
}
} // namespace Doom
