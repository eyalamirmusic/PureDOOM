#include "MapAction.h"

#include "../Host/Platform.h" // doom_abs
#include "../Game/MapSpawns.h" // leveltime
#include "Random.h"
#include "SimDefs.h"

#include "../Game/LevelStats.h"
#include "../Game/SkyState.h"
#include "../Game/SoundData.h"

#include "Clip.h"
#include "ActionScratch.h"
#include "MapUtil.h"
#include "Movement.h"

#include "Specials.h"
#include "../Game/Sound.h"
#include "../Host/System.h"
#include "../Render/Main.h"
#include "Interaction.h"
#include "Mobj.h"
#include "Sight.h"
#include "Switches.h"
#include "Random.h"
#include "../Math/BBox.h"
namespace Doom
{
namespace
{
//
// The p_map action scratch now lives on the Engine (Sim/ActionScratch.h, moved by
// the file-scope-statics sweep - REFACTOR.md, Step 5). slideMove is the one action
// still threaded through a bare function pointer (PTR_SlideTraverse), so it alone
// still needs a home there rather than a capture; each function hoists its own
// `auto& scratch = actionScratch();` once, rather than reaching the members through
// file-scope reference aliases (REFACTOR.md, Step 9 strand (a)).

//
// P_HitSlideLine
// Adjusts the xmove / ymove so that the next move will slide along the wall.
//
void hitSlideLine(Line* ld)
{
    auto& scratch = actionScratch();

    int side;

    angle_t lineangle;
    angle_t moveangle;
    angle_t deltaangle;

    fixed_t movelen;
    fixed_t newlen;

    if (ld->slopetype == ST_HORIZONTAL)
    {
        scratch.tmymove = fixed_t {};
        return;
    }

    if (ld->slopetype == ST_VERTICAL)
    {
        scratch.tmxmove = fixed_t {};
        return;
    }

    side = lineSide({scratch.slidemo->x, scratch.slidemo->y}, *ld);

    lineangle = Doom::pointToAngle2(fixed_t {}, fixed_t {}, ld->dx, ld->dy);

    if (side == 1)
        lineangle += ANG180;

    moveangle = Doom::pointToAngle2(
        fixed_t {}, fixed_t {}, scratch.tmxmove, scratch.tmymove);
    deltaangle = moveangle - lineangle;

    if (deltaangle > ANG180)
        deltaangle += ANG180;

    const auto lineangleFine = lineangle.fineIndex();
    const auto deltaangleFine = deltaangle.fineIndex();

    movelen = approxDistance(scratch.tmxmove, scratch.tmymove);
    newlen = FixedMul(movelen, finecosine[deltaangleFine]);

    scratch.tmxmove = FixedMul(newlen, finecosine[lineangleFine]);
    scratch.tmymove = FixedMul(newlen, finesine[lineangleFine]);
}

//
// PTR_SlideTraverse
//
bool slideTraverse(Intercept* in)
{
    Clip& clip = Doom::clip();
    auto& scratch = actionScratch();

    Line* li;

    if (!in->isaline)
        fatalError("Error: PTR_SlideTraverse: not a line?");

    li = in->d.line;

    if (!(li->flags & ML_TWOSIDED))
    {
        if (lineSide({scratch.slidemo->x, scratch.slidemo->y}, *li))
        {
            // don't hit the back side
            return true;
        }
        goto isblocking;
    }

    // set openrange, opentop, openbottom
    updateLineOpening(*li);

    if (clip.openrange < scratch.slidemo->height)
        goto isblocking; // doesn't fit

    if (clip.opentop - scratch.slidemo->z < scratch.slidemo->height)
        goto isblocking; // mobj is too high

    if (clip.openbottom - scratch.slidemo->z > 24 * FRACUNIT)
        goto isblocking; // too big a step up

    // this line doesn't block movement
    return true;

    // the line does block movement,
    // see if it is closer than best so far
isblocking:
    if (in->frac < scratch.bestslidefrac)
    {
        scratch.bestslidefrac = in->frac;
        scratch.bestslideline = li;
    }

    return false; // stop
}

//
// PTR_AimTraverse
// Finds the auto-aim target and slope, if any. The hitscan's own scratch (the
// shooter, the shot's z, the narrowing slope window) rides along as captures from
// aimLineAttack now, and the result - which used to be Clip::linetarget/aimslope -
// is written into `result` by reference (REFACTOR.md, Step 9 strand (a)).
//
bool aimTraverse(Intercept* in,
                 Mobj* shootthing,
                 fixed_t shootz,
                 fixed_t& topslope,
                 fixed_t& bottomslope,
                 AimResult& result)
{
    Clip& clip = Doom::clip();

    Line* li;
    Mobj* th;
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
        updateLineOpening(*li);

        if (clip.openbottom >= clip.opentop)
            return false; // stop

        dist = FixedMul(clip.attackrange, in->frac);

        if (li->frontsector->floorheight != li->backsector->floorheight)
        {
            slope = FixedDiv(clip.openbottom - shootz, dist);
            if (slope > bottomslope)
                bottomslope = slope;
        }

        if (li->frontsector->ceilingheight != li->backsector->ceilingheight)
        {
            slope = FixedDiv(clip.opentop - shootz, dist);
            if (slope < topslope)
                topslope = slope;
        }

        if (topslope <= bottomslope)
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

    if (thingtopslope < bottomslope)
        return true; // shot over the thing

    thingbottomslope = FixedDiv(th->z - shootz, dist);

    if (thingbottomslope > topslope)
        return true; // shot under the thing

    // this thing can be hit!
    if (thingtopslope > topslope)
        thingtopslope = topslope;

    if (thingbottomslope < bottomslope)
        thingbottomslope = bottomslope;

    result.slope = (thingtopslope + thingbottomslope) / 2;
    result.target = th;

    return false; // don't go any farther
}

//
// PTR_ShootTraverse
// The hitscan's own scratch (the shooter, the shot's z, the fixed aim slope and
// the damage) rides along as captures from lineAttack now (REFACTOR.md, Step 9
// strand (a)).
//
bool shootTraverse(
    Intercept* in, Mobj* shootthing, fixed_t shootz, fixed_t aimslope, int la_damage)
{
    Clip& clip = Doom::clip();

    fixed_t x;
    fixed_t y;
    fixed_t z;
    fixed_t frac;

    Line* li;

    Mobj* th;

    fixed_t slope;
    fixed_t dist;
    fixed_t thingtopslope;
    fixed_t thingbottomslope;

    auto& sky = skyState();

    if (in->isaline)
    {
        li = in->d.line;

        if (li->special)
            Doom::shootSpecialLine(shootthing, li);

        if (!(li->flags & ML_TWOSIDED))
            goto hitline;

        // crosses a two sided line
        updateLineOpening(*li);

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
        x = clip.trace.origin.x + FixedMul(clip.trace.delta.x, frac);
        y = clip.trace.origin.y + FixedMul(clip.trace.delta.y, frac);
        z = shootz + FixedMul(aimslope, FixedMul(frac, clip.attackrange));

        if (li->frontsector->ceilingpic == sky.skyflatnum)
        {
            // don't shoot the sky!
            if (z > li->frontsector->ceilingheight)
                return false;

            // it's a sky hack wall
            if (li->backsector && li->backsector->ceilingpic == sky.skyflatnum)
                return false;
        }

        // Spawn bullet puffs.
        Doom::spawnPuff(x, y, z);

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

    x = clip.trace.origin.x + FixedMul(clip.trace.delta.x, frac);
    y = clip.trace.origin.y + FixedMul(clip.trace.delta.y, frac);
    z = shootz + FixedMul(aimslope, FixedMul(frac, clip.attackrange));

    // Spawn bullet puffs or blod spots, depending on target type.
    if (in->d.thing->flags & MF_NOBLOOD)
        Doom::spawnPuff(x, y, z);
    else
        Doom::spawnBlood(x, y, z, la_damage);

    if (la_damage)
        Doom::damageMobj(th, shootthing, shootthing, la_damage);

    // don't go any farther
    return false;
}

//
// USE LINES
//

//
// The per-line half of Doom::useLines. `usething` was a global purely so this
// could see who pressed use; it is a capture now.
//
static bool useTraverse(Intercept* in, Mobj* usething)
{
    Clip& clip = Doom::clip();

    int side;

    if (!in->d.line->special)
    {
        updateLineOpening(*in->d.line);
        if (!clip.openrange.isPositive())
        {
            Doom::startSound(usething, sfx_noway);

            // can't use through a wall
            return false;
        }
        // not a special line, but keep checking
        return true;
    }

    side = 0;
    if (lineSide({usething->x, usething->y}, *in->d.line) == 1)
        side = 1;

    Doom::useSpecialLine(usething, in->d.line, side);

    // can't use for than one special line in a row
    return false;
}

//
//
// The per-thing half of Doom::radiusAttack. Vanilla passed the bomb's spot,
// source and damage through three globals because a bare function pointer
// cannot carry context; it is a lambda over those three now, so the globals are
// gone (REFACTOR.md, Step 9).
//
static bool
    radiusAttackThing(Mobj* thing, Mobj* bombspot, Mobj* bombsource, int bombdamage)
{
    fixed_t dx;
    fixed_t dy;
    fixed_t spread;

    if (!(thing->flags & MF_SHOOTABLE))
        return true;

    // Boss spider and cyborg take no damage from concussion.
    if (thing->type == MT_CYBORG || thing->type == MT_SPIDER)
        return true;

    dx = doom_abs(thing->x - bombspot->x);
    dy = doom_abs(thing->y - bombspot->y);

    spread = dx > dy ? dx : dy;

    // Whole map units from here on - the falloff is subtracted from the damage,
    // which is a plain int. Vanilla keeps this in its fixed_t `dist` after the
    // shift; the shift is what makes it an integer distance.
    int dist = (spread - thing->radius).toInt();

    if (dist < 0)
        dist = 0;

    if (dist >= bombdamage)
        return true; // out of range

    if (Doom::checkSight(thing, bombspot))
    {
        // must be in direct path
        Doom::damageMobj(thing, bombspot, bombsource, bombdamage - dist);
    }

    return true;
}

//
// PIT_ChangeSector
//
//
// The per-thing half of Doom::changeSector. crushchange was a global carrying
// context in and nofit a global carrying the answer back out; they are a capture
// and an out-parameter now.
//
static bool changeSectorThing(Mobj* thing, bool crushchange, bool& nofit)
{
    Mobj* mo;

    if (thingHeightClip(thing))
    {
        // keep checking
        return true;
    }

    // crunch bodies to giblets
    if (thing->health <= 0)
    {
        Doom::setMobjState(thing, S_GIBS);

        thing->flags &= ~MF_SOLID;
        thing->height = fixed_t {};
        thing->radius = fixed_t {};

        // keep checking
        return true;
    }

    // crunch dropped items
    if (thing->flags & MF_DROPPED)
    {
        Doom::removeMobj(thing);

        // keep checking
        return true;
    }

    if (!(thing->flags & MF_SHOOTABLE))
    {
        // assume it is bloody gibs or something
        return true;
    }

    nofit = true;

    if (crushchange && !(levelStats().leveltime & 3))
    {
        Doom::damageMobj(thing, nullptr, nullptr, 10);

        // spray blood in a random direction
        mo = Doom::spawnMobj(
            thing->x, thing->y, thing->z + thing->height / 2, MT_BLOOD);

        // Raw: the random difference shifted into the fraction, ~+-16 units/tic.
        mo->momx = fixed_t {
            (Doom::randomness().forPlay() - Doom::randomness().forPlay()) << 12};
        mo->momy = fixed_t {
            (Doom::randomness().forPlay() - Doom::randomness().forPlay()) << 12};
    }

    // keep checking (crush other things)
    return true;
}
} // namespace

//
// Doom::slideMove
// The momx / momy move is bad, so try to slide along a wall. Find the first line
// hit, move flush to it, and slide along it. This is a kludgy mess.
//
void slideMove(Mobj* mo)
{
    auto& scratch = actionScratch();

    fixed_t leadx;
    fixed_t leady;
    fixed_t trailx;
    fixed_t traily;
    fixed_t newx;
    fixed_t newy;
    int hitcount;

    scratch.slidemo = mo;
    hitcount = 0;

retry:
    if (++hitcount == 3)
        goto stairstep; // don't loop forever

    // trace along the three leading corners
    if (mo->momx.isPositive())
    {
        leadx = mo->x + mo->radius;
        trailx = mo->x - mo->radius;
    }
    else
    {
        leadx = mo->x - mo->radius;
        trailx = mo->x + mo->radius;
    }

    if (mo->momy.isPositive())
    {
        leady = mo->y + mo->radius;
        traily = mo->y - mo->radius;
    }
    else
    {
        leady = mo->y - mo->radius;
        traily = mo->y + mo->radius;
    }

    scratch.bestslidefrac = FRACUNIT + fixed_t {1};

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
    if (scratch.bestslidefrac == FRACUNIT + fixed_t {1})
    {
        // the move most have hit the middle, so stairstep
    stairstep:
        if (!tryMove(mo, mo->x, mo->y + mo->momy))
            tryMove(mo, mo->x + mo->momx, mo->y);
        return;
    }

    // fudge a bit to make sure it doesn't hit
    scratch.bestslidefrac -= fixed_t {0x800};
    if (scratch.bestslidefrac.isPositive())
    {
        newx = FixedMul(mo->momx, scratch.bestslidefrac);
        newy = FixedMul(mo->momy, scratch.bestslidefrac);

        if (!tryMove(mo, mo->x + newx, mo->y + newy))
            goto stairstep;
    }

    // Now continue along the wall.
    // First calculate remainder.
    scratch.bestslidefrac = FRACUNIT - (scratch.bestslidefrac + fixed_t {0x800});

    if (scratch.bestslidefrac > FRACUNIT)
        scratch.bestslidefrac = FRACUNIT;

    if (!scratch.bestslidefrac.isPositive())
        return;

    scratch.tmxmove = FixedMul(mo->momx, scratch.bestslidefrac);
    scratch.tmymove = FixedMul(mo->momy, scratch.bestslidefrac);

    hitSlideLine(scratch.bestslideline); // clip the moves

    mo->momx = scratch.tmxmove;
    mo->momy = scratch.tmymove;

    if (!tryMove(mo, mo->x + scratch.tmxmove, mo->y + scratch.tmymove))
    {
        goto retry;
    }
}

//
// Doom::aimLineAttack
//
AimResult aimLineAttack(Mobj* t1, angle_t angle, fixed_t distance)
{
    Clip& clip = Doom::clip();

    fixed_t x2;
    fixed_t y2;

    const auto angleFine = angle.fineIndex();
    Mobj* shootthing = t1;

    // The range in WHOLE units scales the fixed cosine: an integer product, not a
    // fixed-point one. FixedMul here would divide the reach by 65536.
    x2 = t1->x + distance.toInt() * finecosine[angleFine];
    y2 = t1->y + distance.toInt() * finesine[angleFine];
    fixed_t shootz = t1->z + (t1->height >> 1) + 8 * FRACUNIT;

    // can't shoot outside view angles
    fixed_t topslope = 100 * FRACUNIT / 160;
    fixed_t bottomslope = -100 * FRACUNIT / 160;

    clip.attackrange = distance;

    AimResult result;

    const auto tryAim =
        [shootthing, shootz, &topslope, &bottomslope, &result](Intercept* in)
    { return aimTraverse(in, shootthing, shootz, topslope, bottomslope, result); };

    pathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES | PT_ADDTHINGS, tryAim);

    return result;
}

//
// Doom::lineAttack
// If damage == 0, it is just a test trace used only to find an aim target.
//
void lineAttack(Mobj* t1, angle_t angle, fixed_t distance, fixed_t slope, int damage)
{
    Clip& clip = Doom::clip();

    fixed_t x2;
    fixed_t y2;

    const auto angleFine = angle.fineIndex();
    Mobj* shootthing = t1;
    int la_damage = damage;
    // Whole units scaling the fixed cosine - an integer product. See aimLineAttack.
    x2 = t1->x + distance.toInt() * finecosine[angleFine];
    y2 = t1->y + distance.toInt() * finesine[angleFine];
    fixed_t shootz = t1->z + (t1->height >> 1) + 8 * FRACUNIT;
    clip.attackrange = distance;
    fixed_t aimslope = slope;

    const auto tryShoot = [shootthing, shootz, aimslope, la_damage](Intercept* in)
    { return shootTraverse(in, shootthing, shootz, aimslope, la_damage); };

    pathTraverse(t1->x, t1->y, x2, y2, PT_ADDLINES | PT_ADDTHINGS, tryShoot);
}

//
// Doom::useLines
// Looks for special lines in front of the player to activate.
//
void useLines(Player* player)
{
    angle_t angle;
    fixed_t x1;
    fixed_t y1;
    fixed_t x2;
    fixed_t y2;

    angle = player->mo->angle;
    const auto angleFine = angle.fineIndex();

    x1 = player->mo->x;
    y1 = player->mo->y;
    // USERANGE in whole units scaling the fixed cosine - an integer product.
    x2 = x1 + USERANGE.toInt() * finecosine[angleFine];
    y2 = y1 + USERANGE.toInt() * finesine[angleFine];

    Mobj* usething = player->mo;

    const auto tryLine = [usething](Intercept* in)
    { return useTraverse(in, usething); };

    pathTraverse(x1, y1, x2, y2, PT_ADDLINES, tryLine);
}

//
// Doom::radiusAttack
// Source is the creature that caused the explosion at spot.
//
void radiusAttack(Mobj* spot, Mobj* source, int damage)
{
    int xl;
    int xh;
    int yl;
    int yh;

    // Vanilla, overflow and all: MAXRADIUS is already a fixed value, so shifting
    // (damage + MAXRADIUS) up by another FRACBITS wraps the MAXRADIUS term away
    // and leaves the damage alone as the radius. Kept as the integer expression
    // it has always been - the demos are recorded against the wrap.
    fixed_t dist {(damage + (MAXRADIUS).raw) << FRACBITS};

    // Blockmap cell indices: a raw shift by MAPBLOCKSHIFT, not a conversion.
    yh = (spot->y + dist - bmaporgy).raw >> MAPBLOCKSHIFT;
    yl = (spot->y - dist - bmaporgy).raw >> MAPBLOCKSHIFT;
    xh = (spot->x + dist - bmaporgx).raw >> MAPBLOCKSHIFT;
    xl = (spot->x - dist - bmaporgx).raw >> MAPBLOCKSHIFT;
    const auto hitThing = [spot, source, damage](Mobj* thing)
    { return radiusAttackThing(thing, spot, source, damage); };

    for (int y = yl; y <= yh; y++)
        for (int x = xl; x <= xh; x++)
            forEachThingInBlock(x, y, hitThing);
}

//
// Doom::changeSector
//
bool changeSector(Sector* sector, bool crunch)
{
    bool nofit = false;

    const auto clipThing = [crunch, &nofit](Mobj* thing)
    { return changeSectorThing(thing, crunch, nofit); };

    // re-check heights for all things near the moving sector
    for (int x = sector->blockbox[BOXLEFT]; x <= sector->blockbox[BOXRIGHT]; x++)
        for (int y = sector->blockbox[BOXBOTTOM]; y <= sector->blockbox[BOXTOP]; y++)
            forEachThingInBlock(x, y, clipThing);

    return nofit;
}
} // namespace Doom
