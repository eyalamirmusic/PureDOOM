#include "MapAction.h"

#include "../doom_config.h" // doom_abs
#include "../doomstat.h" // leveltime
#include "../i_system.h"
#include "../m_bbox.h"
#include "../m_random.h"
#include "../p_local.h"
#include "../r_state.h" // skyflatnum
#include "../s_sound.h"
#include "../sounds.h"

#include "Clip.h"
#include "ActionScratch.h"
#include "MapUtil.h"
#include "Movement.h"

namespace Doom
{
namespace
{
//
// The p_map action scratch now lives on the Engine (Sim/ActionScratch.h, moved by the
// file-scope-statics sweep - REFACTOR.md, Step 5). The vanilla names below (grouped by the action
// that uses them: slide, hitscan, use, radius, change-sector) are references onto that member; read
// by no other file.

// SLIDE MOVE scratch - the best (and second-best) wall the momentum hit, and the
// move being clipped along it.
//
static fixed_t& bestslidefrac = actionScratch().bestslidefrac;
static fixed_t& secondslidefrac = actionScratch().secondslidefrac;

static line_t*& bestslideline = actionScratch().bestslideline;
static line_t*& secondslideline = actionScratch().secondslideline;

static mobj_t*& slidemo = actionScratch().slidemo;

static fixed_t& tmxmove = actionScratch().tmxmove;
static fixed_t& tmymove = actionScratch().tmymove;

//
// P_HitSlideLine
// Adjusts the xmove / ymove so that the next move will slide along the wall.
//
void hitSlideLine(line_t* ld)
{
    int side;

    angle_t lineangle;
    angle_t moveangle;
    angle_t deltaangle;

    fixed_t movelen;
    fixed_t newlen;

    if (ld->slopetype == ST_HORIZONTAL)
    {
        tmymove = 0;
        return;
    }

    if (ld->slopetype == ST_VERTICAL)
    {
        tmxmove = 0;
        return;
    }

    side = P_PointOnLineSide(slidemo->x, slidemo->y, ld);

    lineangle = R_PointToAngle2(0, 0, ld->dx, ld->dy);

    if (side == 1)
        lineangle += ANG180;

    moveangle = R_PointToAngle2(0, 0, tmxmove, tmymove);
    deltaangle = moveangle - lineangle;

    if (deltaangle > ANG180)
        deltaangle += ANG180;

    lineangle >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;

    movelen = P_AproxDistance(tmxmove, tmymove);
    newlen = FixedMul(movelen, finecosine[deltaangle]);

    tmxmove = FixedMul(newlen, finecosine[lineangle]);
    tmymove = FixedMul(newlen, finesine[lineangle]);
}

//
// PTR_SlideTraverse
//
doom_boolean slideTraverse(intercept_t* in)
{
    Clip& clip = Doom::clip();

    line_t* li;

    if (!in->isaline)
        I_Error("Error: PTR_SlideTraverse: not a line?");

    li = in->d.line;

    if (!(li->flags & ML_TWOSIDED))
    {
        if (P_PointOnLineSide(slidemo->x, slidemo->y, li))
        {
            // don't hit the back side
            return true;
        }
        goto isblocking;
    }

    // set openrange, opentop, openbottom
    P_LineOpening(li);

    if (clip.openrange < slidemo->height)
        goto isblocking; // doesn't fit

    if (clip.opentop - slidemo->z < slidemo->height)
        goto isblocking; // mobj is too high

    if (clip.openbottom - slidemo->z > 24 * FRACUNIT)
        goto isblocking; // too big a step up

    // this line doesn't block movement
    return true;

    // the line does block movement,
    // see if it is closer than best so far
isblocking:
    if (in->frac < bestslidefrac)
    {
        secondslidefrac = bestslidefrac;
        secondslideline = bestslideline;
        bestslidefrac = in->frac;
        bestslideline = li;
    }

    return false; // stop
}

//
// PTR_AimTraverse
// Sets linetarget and aimslope when a target is aimed at.
//
static fixed_t& aimslope = actionScratch().aimslope;
static mobj_t*& shootthing = actionScratch().shootthing;

// Height if not aiming up or down.
static fixed_t& shootz = actionScratch().shootz;

static int& la_damage = actionScratch().la_damage;

doom_boolean aimTraverse(intercept_t* in)
{
    Clip& clip = Doom::clip();

    line_t* li;
    mobj_t* th;
    fixed_t slope;
    fixed_t thingtopslope;
    fixed_t thingbottomslope;
    fixed_t dist;

    if (in->isaline)
    {
        li = in->d.line;

        if (!(li->flags & ML_TWOSIDED))
            return false; // stop

        // Crosses a two sided line. A two sided line will restrict the possible
        // target ranges.
        P_LineOpening(li);

        if (clip.openbottom >= clip.opentop)
            return false; // stop

        dist = FixedMul(clip.attackrange, in->frac);

        if (li->frontsector->floorheight != li->backsector->floorheight)
        {
            slope = FixedDiv(clip.openbottom - shootz, dist);
            if (slope > clip.bottomslope)
                clip.bottomslope = slope;
        }

        if (li->frontsector->ceilingheight != li->backsector->ceilingheight)
        {
            slope = FixedDiv(clip.opentop - shootz, dist);
            if (slope < clip.topslope)
                clip.topslope = slope;
        }

        if (clip.topslope <= clip.bottomslope)
            return false; // stop

        return true; // shot continues
    }

    // shoot a thing
    th = in->d.thing;
    if (th == shootthing)
        return true; // can't shoot self

    if (!(th->flags & MF_SHOOTABLE))
        return true; // corpse or something

    // check angles to see if the thing can be aimed at
    dist = FixedMul(clip.attackrange, in->frac);
    thingtopslope = FixedDiv(th->z + th->height - shootz, dist);

    if (thingtopslope < clip.bottomslope)
        return true; // shot over the thing

    thingbottomslope = FixedDiv(th->z - shootz, dist);

    if (thingbottomslope > clip.topslope)
        return true; // shot under the thing

    // this thing can be hit!
    if (thingtopslope > clip.topslope)
        thingtopslope = clip.topslope;

    if (thingbottomslope < clip.bottomslope)
        thingbottomslope = clip.bottomslope;

    aimslope = (thingtopslope + thingbottomslope) / 2;
    clip.linetarget = th;

    return false; // don't go any farther
}

//
// PTR_ShootTraverse
//
doom_boolean shootTraverse(intercept_t* in)
{
    Clip& clip = Doom::clip();

    fixed_t x;
    fixed_t y;
    fixed_t z;
    fixed_t frac;

    line_t* li;

    mobj_t* th;

    fixed_t slope;
    fixed_t dist;
    fixed_t thingtopslope;
    fixed_t thingbottomslope;

    if (in->isaline)
    {
        li = in->d.line;

        if (li->special)
            P_ShootSpecialLine(shootthing, li);

        if (!(li->flags & ML_TWOSIDED))
            goto hitline;

        // crosses a two sided line
        P_LineOpening(li);

        dist = FixedMul(clip.attackrange, in->frac);

        if (li->frontsector->floorheight != li->backsector->floorheight)
        {
            slope = FixedDiv(clip.openbottom - shootz, dist);
            if (slope > aimslope)
                goto hitline;
        }

        if (li->frontsector->ceilingheight != li->backsector->ceilingheight)
        {
            slope = FixedDiv(clip.opentop - shootz, dist);
            if (slope < aimslope)
                goto hitline;
        }

        // shot continues
        return true;

        // hit line
    hitline:
        // position a bit closer
        frac = in->frac - FixedDiv(4 * FRACUNIT, clip.attackrange);
        x = clip.trace.x + FixedMul(clip.trace.dx, frac);
        y = clip.trace.y + FixedMul(clip.trace.dy, frac);
        z = shootz + FixedMul(aimslope, FixedMul(frac, clip.attackrange));

        if (li->frontsector->ceilingpic == skyflatnum)
        {
            // don't shoot the sky!
            if (z > li->frontsector->ceilingheight)
                return false;

            // it's a sky hack wall
            if (li->backsector && li->backsector->ceilingpic == skyflatnum)
                return false;
        }

        // Spawn bullet puffs.
        P_SpawnPuff(x, y, z);

        // don't go any farther
        return false;
    }

    // shoot a thing
    th = in->d.thing;
    if (th == shootthing)
        return true; // can't shoot self

    if (!(th->flags & MF_SHOOTABLE))
        return true; // corpse or something

    // check angles to see if the thing can be aimed at
    dist = FixedMul(clip.attackrange, in->frac);
    thingtopslope = FixedDiv(th->z + th->height - shootz, dist);

    if (thingtopslope < aimslope)
        return true; // shot over the thing

    thingbottomslope = FixedDiv(th->z - shootz, dist);

    if (thingbottomslope > aimslope)
        return true; // shot under the thing

    // hit thing
    // position a bit closer
    frac = in->frac - FixedDiv(10 * FRACUNIT, clip.attackrange);

    x = clip.trace.x + FixedMul(clip.trace.dx, frac);
    y = clip.trace.y + FixedMul(clip.trace.dy, frac);
    z = shootz + FixedMul(aimslope, FixedMul(frac, clip.attackrange));

    // Spawn bullet puffs or blod spots, depending on target type.
    if (in->d.thing->flags & MF_NOBLOOD)
        P_SpawnPuff(x, y, z);
    else
        P_SpawnBlood(x, y, z, la_damage);

    if (la_damage)
        P_DamageMobj(th, shootthing, shootthing, la_damage);

    // don't go any farther
    return false;
}

//
// USE LINES
//
static mobj_t*& usething = actionScratch().usething;

doom_boolean useTraverse(intercept_t* in)
{
    Clip& clip = Doom::clip();

    int side;

    if (!in->d.line->special)
    {
        P_LineOpening(in->d.line);
        if (clip.openrange <= 0)
        {
            S_StartSound(usething, sfx_noway);

            // can't use through a wall
            return false;
        }
        // not a special line, but keep checking
        return true;
    }

    side = 0;
    if (P_PointOnLineSide(usething->x, usething->y, in->d.line) == 1)
        side = 1;

    P_UseSpecialLine(usething, in->d.line, side);

    // can't use for than one special line in a row
    return false;
}

//
// RADIUS ATTACK scratch
//
static mobj_t*& bombsource = actionScratch().bombsource;
static mobj_t*& bombspot = actionScratch().bombspot;
static int& bombdamage = actionScratch().bombdamage;

//
// PIT_RadiusAttack
// "bombsource" is the creature that caused the explosion at "bombspot".
//
doom_boolean radiusAttackThing(mobj_t* thing)
{
    fixed_t dx;
    fixed_t dy;
    fixed_t dist;

    if (!(thing->flags & MF_SHOOTABLE))
        return true;

    // Boss spider and cyborg take no damage from concussion.
    if (thing->type == MT_CYBORG || thing->type == MT_SPIDER)
        return true;

    dx = doom_abs(thing->x - bombspot->x);
    dy = doom_abs(thing->y - bombspot->y);

    dist = dx > dy ? dx : dy;
    dist = (dist - thing->radius) >> FRACBITS;

    if (dist < 0)
        dist = 0;

    if (dist >= bombdamage)
        return true; // out of range

    if (P_CheckSight(thing, bombspot))
    {
        // must be in direct path
        P_DamageMobj(thing, bombspot, bombsource, bombdamage - dist);
    }

    return true;
}

//
// PIT_ChangeSector
//
static doom_boolean& nofit = actionScratch().nofit;
static doom_boolean& crushchange = actionScratch().crushchange;

doom_boolean changeSectorThing(mobj_t* thing)
{
    mobj_t* mo;

    if (thingHeightClip(thing))
    {
        // keep checking
        return true;
    }

    // crunch bodies to giblets
    if (thing->health <= 0)
    {
        P_SetMobjState(thing, S_GIBS);

        thing->flags &= ~MF_SOLID;
        thing->height = 0;
        thing->radius = 0;

        // keep checking
        return true;
    }

    // crunch dropped items
    if (thing->flags & MF_DROPPED)
    {
        P_RemoveMobj(thing);

        // keep checking
        return true;
    }

    if (!(thing->flags & MF_SHOOTABLE))
    {
        // assume it is bloody gibs or something
        return true;
    }

    nofit = true;

    if (crushchange && !(leveltime & 3))
    {
        P_DamageMobj(thing, nullptr, nullptr, 10);

        // spray blood in a random direction
        mo = P_SpawnMobj(thing->x, thing->y, thing->z + thing->height / 2, MT_BLOOD);

        mo->momx = (P_Random() - P_Random()) << 12;
        mo->momy = (P_Random() - P_Random()) << 12;
    }

    // keep checking (crush other things)
    return true;
}
} // namespace

//
// P_SlideMove
// The momx / momy move is bad, so try to slide along a wall. Find the first line
// hit, move flush to it, and slide along it. This is a kludgy mess.
//
void slideMove(mobj_t* mo)
{
    fixed_t leadx;
    fixed_t leady;
    fixed_t trailx;
    fixed_t traily;
    fixed_t newx;
    fixed_t newy;
    int hitcount;

    slidemo = mo;
    hitcount = 0;

retry:
    if (++hitcount == 3)
        goto stairstep; // don't loop forever

    // trace along the three leading corners
    if (mo->momx > 0)
    {
        leadx = mo->x + mo->radius;
        trailx = mo->x - mo->radius;
    }
    else
    {
        leadx = mo->x - mo->radius;
        trailx = mo->x + mo->radius;
    }

    if (mo->momy > 0)
    {
        leady = mo->y + mo->radius;
        traily = mo->y - mo->radius;
    }
    else
    {
        leady = mo->y - mo->radius;
        traily = mo->y + mo->radius;
    }

    bestslidefrac = FRACUNIT + 1;

    pathTraverse(leadx,
                 leady,
                 leadx + mo->momx,
                 leady + mo->momy,
                 PT_ADDLINES,
                 slideTraverse);
    pathTraverse(trailx,
                 leady,
                 trailx + mo->momx,
                 leady + mo->momy,
                 PT_ADDLINES,
                 slideTraverse);
    pathTraverse(leadx,
                 traily,
                 leadx + mo->momx,
                 traily + mo->momy,
                 PT_ADDLINES,
                 slideTraverse);

    // move up to the wall
    if (bestslidefrac == FRACUNIT + 1)
    {
        // the move most have hit the middle, so stairstep
    stairstep:
        if (!tryMove(mo, mo->x, mo->y + mo->momy))
            tryMove(mo, mo->x + mo->momx, mo->y);
        return;
    }

    // fudge a bit to make sure it doesn't hit
    bestslidefrac -= 0x800;
    if (bestslidefrac > 0)
    {
        newx = FixedMul(mo->momx, bestslidefrac);
        newy = FixedMul(mo->momy, bestslidefrac);

        if (!tryMove(mo, mo->x + newx, mo->y + newy))
            goto stairstep;
    }

    // Now continue along the wall.
    // First calculate remainder.
    bestslidefrac = FRACUNIT - (bestslidefrac + 0x800);

    if (bestslidefrac > FRACUNIT)
        bestslidefrac = FRACUNIT;

    if (bestslidefrac <= 0)
        return;

    tmxmove = FixedMul(mo->momx, bestslidefrac);
    tmymove = FixedMul(mo->momy, bestslidefrac);

    hitSlideLine(bestslideline); // clip the moves

    mo->momx = tmxmove;
    mo->momy = tmymove;

    if (!tryMove(mo, mo->x + tmxmove, mo->y + tmymove))
    {
        goto retry;
    }
}

//
// P_AimLineAttack
//
fixed_t aimLineAttack(mobj_t* t1, angle_t angle, fixed_t distance)
{
    Clip& clip = Doom::clip();

    fixed_t x2;
    fixed_t y2;

    angle >>= ANGLETOFINESHIFT;
    shootthing = t1;

    x2 = t1->x + (distance >> FRACBITS) * finecosine[angle];
    y2 = t1->y + (distance >> FRACBITS) * finesine[angle];
    shootz = t1->z + (t1->height >> 1) + 8 * FRACUNIT;

    // can't shoot outside view angles
    clip.topslope = 100 * FRACUNIT / 160;
    clip.bottomslope = -100 * FRACUNIT / 160;

    clip.attackrange = distance;
    clip.linetarget = nullptr;

    pathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES | PT_ADDTHINGS, aimTraverse);

    if (clip.linetarget)
        return aimslope;

    return 0;
}

//
// P_LineAttack
// If damage == 0, it is just a test trace that will leave linetarget set.
//
void lineAttack(
    mobj_t* t1, angle_t angle, fixed_t distance, fixed_t slope, int damage)
{
    Clip& clip = Doom::clip();

    fixed_t x2;
    fixed_t y2;

    angle >>= ANGLETOFINESHIFT;
    shootthing = t1;
    la_damage = damage;
    x2 = t1->x + (distance >> FRACBITS) * finecosine[angle];
    y2 = t1->y + (distance >> FRACBITS) * finesine[angle];
    shootz = t1->z + (t1->height >> 1) + 8 * FRACUNIT;
    clip.attackrange = distance;
    aimslope = slope;

    pathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES | PT_ADDTHINGS, shootTraverse);
}

//
// P_UseLines
// Looks for special lines in front of the player to activate.
//
void useLines(player_t* player)
{
    int angle;
    fixed_t x1;
    fixed_t y1;
    fixed_t x2;
    fixed_t y2;

    usething = player->mo;

    angle = player->mo->angle >> ANGLETOFINESHIFT;

    x1 = player->mo->x;
    y1 = player->mo->y;
    x2 = x1 + (USERANGE >> FRACBITS) * finecosine[angle];
    y2 = y1 + (USERANGE >> FRACBITS) * finesine[angle];

    pathTraverse(x1, y1, x2, y2, PT_ADDLINES, useTraverse);
}

//
// P_RadiusAttack
// Source is the creature that caused the explosion at spot.
//
void radiusAttack(mobj_t* spot, mobj_t* source, int damage)
{
    int xl;
    int xh;
    int yl;
    int yh;

    fixed_t dist;

    dist = (damage + MAXRADIUS) << FRACBITS;
    yh = (spot->y + dist - bmaporgy) >> MAPBLOCKSHIFT;
    yl = (spot->y - dist - bmaporgy) >> MAPBLOCKSHIFT;
    xh = (spot->x + dist - bmaporgx) >> MAPBLOCKSHIFT;
    xl = (spot->x - dist - bmaporgx) >> MAPBLOCKSHIFT;
    bombspot = spot;
    bombsource = source;
    bombdamage = damage;

    for (int y = yl; y <= yh; y++)
        for (int x = xl; x <= xh; x++)
            forEachThingInBlock(x, y, radiusAttackThing);
}

//
// P_ChangeSector
//
bool changeSector(sector_t* sector, doom_boolean crunch)
{
    nofit = false;
    crushchange = crunch;

    // re-check heights for all things near the moving sector
    for (int x = sector->blockbox[BOXLEFT]; x <= sector->blockbox[BOXRIGHT]; x++)
        for (int y = sector->blockbox[BOXBOTTOM]; y <= sector->blockbox[BOXTOP]; y++)
            forEachThingInBlock(x, y, changeSectorThing);

    return nofit;
}
} // namespace Doom
