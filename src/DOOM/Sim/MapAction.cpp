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

    if (ld->slopetype == SlopeType::Horizontal)
    {
        scratch.tmmove.y = Fixed {};
        return;
    }

    if (ld->slopetype == SlopeType::Vertical)
    {
        scratch.tmmove.x = Fixed {};
        return;
    }

    auto side = ld->pointSide({scratch.slidemo->pos.x, scratch.slidemo->pos.y});

    auto lineangle = pointToAngle2({}, ld->delta);

    if (side == 1)
        lineangle += ang180;

    auto moveangle = pointToAngle2({}, {scratch.tmmove.x, scratch.tmmove.y});
    auto deltaangle = moveangle - lineangle;

    if (deltaangle > ang180)
        deltaangle += ang180;

    const auto lineangleFine = lineangle.fineIndex();
    const auto deltaangleFine = deltaangle.fineIndex();

    auto movelen = approxDistance({scratch.tmmove.x, scratch.tmmove.y});
    auto newlen = FixedMul(movelen, finecosine()[deltaangleFine]);

    scratch.tmmove.x = FixedMul(newlen, finecosine()[lineangleFine]);
    scratch.tmmove.y = FixedMul(newlen, finesine()[lineangleFine]);
}

//
// PTR_SlideTraverse
//
bool slideTraverse(Intercept* in)
{
    auto& clip = clipping();
    auto& scratch = actionScratch();

    if (!in->isaline)
        fatalError("Error: PTR_SlideTraverse: not a line?");

    auto* li = in->d.line;

    // The line does block movement: see if it is closer than the best so far,
    // then stop the traversal.
    auto isblocking = [&]
    {
        if (in->frac < scratch.bestslidefrac)
        {
            scratch.bestslidefrac = in->frac;
            scratch.bestslideline = li;
        }

        return false; // stop
    };

    if (!(li->flags & ML_TWOSIDED))
    {
        if (li->pointSide({scratch.slidemo->pos.x, scratch.slidemo->pos.y}))
        {
            // don't hit the back side
            return true;
        }
        return isblocking();
    }

    // set openrange, opentop, openbottom
    li->updateOpening();

    if (clip.openrange < scratch.slidemo->height)
        return isblocking(); // doesn't fit

    if (clip.opentop - scratch.slidemo->pos.z < scratch.slidemo->height)
        return isblocking(); // mobj is too high

    if (clip.openbottom - scratch.slidemo->pos.z > 24 * FRACUNIT)
        return isblocking(); // too big a step up

    // this line doesn't block movement
    return true;
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
                 Fixed shootz,
                 Fixed& topslope,
                 Fixed& bottomslope,
                 AimResult& result)
{
    auto& clip = clipping();

    Fixed slope;
    Fixed dist;

    if (in->isaline)
    {
        auto* li = in->d.line;

        if (!(li->flags & ML_TWOSIDED))
            return false; // stop

        // Crosses a two sided line. A two sided line will restrict the possible
        // target ranges.
        li->updateOpening();

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
    auto* th = in->d.thing;
    if (th == shootthing)
        return true; // can't shoot self

    if (!(hasFlag(th->flags, MobjFlag::Shootable)))
        return true; // corpse or something

    // check angles to see if the thing can be aimed at
    dist = FixedMul(clip.attackrange, in->frac);
    auto thingtopslope = FixedDiv(th->pos.z + th->height - shootz, dist);

    if (thingtopslope < bottomslope)
        return true; // shot over the thing

    auto thingbottomslope = FixedDiv(th->pos.z - shootz, dist);

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
    Intercept* in, Mobj* shootthing, Fixed shootz, Fixed aimslope, int la_damage)
{
    auto& clip = clipping();

    Fixed x;
    Fixed y;
    Fixed z;
    Fixed frac;

    Line* li;

    Mobj* th;

    Fixed slope;
    Fixed dist;
    Fixed thingtopslope;
    Fixed thingbottomslope;

    auto& sky = skyState();

    if (in->isaline)
    {
        li = in->d.line;

        if (li->special)
            li->shootSpecialLine(*shootthing);

        // The shot hits this line: position the impact a bit closer, respect the
        // sky hack, spawn a puff, and stop the traversal.
        auto hitline = [&]
        {
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
            spawnPuff({x, y, z});

            // don't go any farther
            return false;
        };

        if (!(li->flags & ML_TWOSIDED))
            return hitline();

        // crosses a two sided line
        li->updateOpening();

        dist = FixedMul(clip.attackrange, in->frac);

        if (li->frontsector->floorheight != li->backsector->floorheight)
        {
            slope = FixedDiv(clip.openbottom - shootz, dist);
            if (slope > aimslope)
                return hitline();
        }

        if (li->frontsector->ceilingheight != li->backsector->ceilingheight)
        {
            slope = FixedDiv(clip.opentop - shootz, dist);
            if (slope < aimslope)
                return hitline();
        }

        // shot continues
        return true;
    }

    // shoot a thing
    th = in->d.thing;
    if (th == shootthing)
        return true; // can't shoot self

    if (!(hasFlag(th->flags, MobjFlag::Shootable)))
        return true; // corpse or something

    // check angles to see if the thing can be aimed at
    dist = FixedMul(clip.attackrange, in->frac);
    thingtopslope = FixedDiv(th->pos.z + th->height - shootz, dist);

    if (thingtopslope < aimslope)
        return true; // shot over the thing

    thingbottomslope = FixedDiv(th->pos.z - shootz, dist);

    if (thingbottomslope > aimslope)
        return true; // shot under the thing

    // hit thing
    // position a bit closer
    frac = in->frac - FixedDiv(10 * FRACUNIT, clip.attackrange);

    x = clip.trace.origin.x + FixedMul(clip.trace.delta.x, frac);
    y = clip.trace.origin.y + FixedMul(clip.trace.delta.y, frac);
    z = shootz + FixedMul(aimslope, FixedMul(frac, clip.attackrange));

    // Spawn bullet puffs or blod spots, depending on target type.
    if (hasFlag(in->d.thing->flags, MobjFlag::NoBlood))
        spawnPuff({x, y, z});
    else
        spawnBlood({x, y, z}, la_damage);

    if (la_damage)
        th->damage(shootthing, shootthing, la_damage);

    // don't go any farther
    return false;
}

//
// USE LINES
//

//
// The per-line half of useLines. `usething` was a global purely so this
// could see who pressed use; it is a capture now.
//
static bool useTraverse(Intercept* in, Mobj* usething)
{
    auto& clip = clipping();

    if (!in->d.line->special)
    {
        in->d.line->updateOpening();
        if (!clip.openrange.isPositive())
        {
            startSound(usething, SfxEnum::Noway);

            // can't use through a wall
            return false;
        }
        // not a special line, but keep checking
        return true;
    }

    auto side = 0;
    if (in->d.line->pointSide({usething->pos.x, usething->pos.y}) == 1)
        side = 1;

    in->d.line->useSpecialLine(*usething, side);

    // can't use for than one special line in a row
    return false;
}

//
//
// The per-thing half of radiusAttack. Vanilla passed the bomb's spot,
// source and damage through three globals because a bare function pointer
// cannot carry context; it is a lambda over those three now, so the globals are
// gone (REFACTOR.md, Step 9).
//
static bool
    radiusAttackThing(Mobj* thing, Mobj* bombspot, Mobj* bombsource, int bombdamage)
{
    if (!(hasFlag(thing->flags, MobjFlag::Shootable)))
        return true;

    // Boss spider and cyborg take no damage from concussion.
    if (thing->type == MobjType::Cyborg || thing->type == MobjType::Spider)
        return true;

    auto dx = doom_abs(thing->pos.x - bombspot->pos.x);
    auto dy = doom_abs(thing->pos.y - bombspot->pos.y);

    auto spread = dx > dy ? dx : dy;

    // Whole map units from here on - the falloff is subtracted from the damage,
    // which is a plain int. Vanilla keeps this in its Fixed `dist` after the
    // shift; the shift is what makes it an integer distance.
    auto dist = (spread - thing->radius).toInt();

    if (dist < 0)
        dist = 0;

    if (dist >= bombdamage)
        return true; // out of range

    if (checkSight(thing, bombspot))
    {
        // must be in direct path
        thing->damage(bombspot, bombsource, bombdamage - dist);
    }

    return true;
}

//
// PIT_ChangeSector
//
//
// The per-thing half of changeSector. crushchange was a global carrying
// context in and nofit a global carrying the answer back out; they are a capture
// and an out-parameter now.
//
static bool changeSectorThing(Mobj* thing, bool crushchange, bool& nofit)
{
    if (thing->thingHeightClip())
    {
        // keep checking
        return true;
    }

    // crunch bodies to giblets
    if (thing->health <= 0)
    {
        thing->setState(StateNum::Gibs);

        thing->flags = withoutFlags(thing->flags, MobjFlag::Solid);
        thing->height = Fixed {};
        thing->radius = Fixed {};

        // keep checking
        return true;
    }

    // crunch dropped items
    if (hasFlag(thing->flags, MobjFlag::Dropped))
    {
        thing->remove();

        // keep checking
        return true;
    }

    if (!(hasFlag(thing->flags, MobjFlag::Shootable)))
    {
        // assume it is bloody gibs or something
        return true;
    }

    nofit = true;

    if (crushchange && !(levelStats().leveltime & 3))
    {
        thing->damage(nullptr, nullptr, 10);

        // spray blood in a random direction
        auto* mo =
            spawnMobj({thing->pos.x, thing->pos.y, thing->pos.z + thing->height / 2},
                      MobjType::Blood);

        // Raw: the random difference shifted into the fraction, ~+-16 units/tic.
        mo->mom.x = Fixed {(randomness().forPlay() - randomness().forPlay()) << 12};
        mo->mom.y = Fixed {(randomness().forPlay() - randomness().forPlay()) << 12};
    }

    // keep checking (crush other things)
    return true;
}
} // namespace

//
// slideMove
// The momx / momy move is bad, so try to slide along a wall. Find the first line
// hit, move flush to it, and slide along it. This is a kludgy mess.
//
void Mobj::slideMove()
{
    auto& scratch = actionScratch();

    Fixed leadx;
    Fixed leady;
    Fixed trailx;
    Fixed traily;
    Fixed newx;
    Fixed newy;

    scratch.slidemo = this;
    auto hitcount = 0;

    // The move hit the middle, or the retry cap was reached: step along one axis
    // at a time instead of sliding. Reads the mobj's current momentum and position,
    // which is what each vanilla `goto stairstep` did at the point it jumped.
    auto stairstep = [&]
    {
        if (!tryMove({pos.x, pos.y + mom.y}))
            tryMove({pos.x + mom.x, pos.y});
    };

    for (;;) // vanilla's `retry:` loop
    {
        if (++hitcount == 3)
        {
            // don't loop forever
            stairstep();
            return;
        }

        // trace along the three leading corners
        if (mom.x.isPositive())
        {
            leadx = pos.x + radius;
            trailx = pos.x - radius;
        }
        else
        {
            leadx = pos.x - radius;
            trailx = pos.x + radius;
        }

        if (mom.y.isPositive())
        {
            leady = pos.y + radius;
            traily = pos.y - radius;
        }
        else
        {
            leady = pos.y - radius;
            traily = pos.y + radius;
        }

        scratch.bestslidefrac = FRACUNIT + Fixed {1};

        pathTraverse({leadx, leady},
                     {leadx + mom.x, leady + mom.y},
                     PT_ADDLINES,
                     slideTraverse);
        pathTraverse({trailx, leady},
                     {trailx + mom.x, leady + mom.y},
                     PT_ADDLINES,
                     slideTraverse);
        pathTraverse({leadx, traily},
                     {leadx + mom.x, traily + mom.y},
                     PT_ADDLINES,
                     slideTraverse);

        // move up to the wall
        if (scratch.bestslidefrac == FRACUNIT + Fixed {1})
        {
            // the move most have hit the middle, so stairstep
            stairstep();
            return;
        }

        // fudge a bit to make sure it doesn't hit
        scratch.bestslidefrac -= Fixed {0x800};
        if (scratch.bestslidefrac.isPositive())
        {
            newx = FixedMul(mom.x, scratch.bestslidefrac);
            newy = FixedMul(mom.y, scratch.bestslidefrac);

            if (!tryMove({pos.x + newx, pos.y + newy}))
            {
                stairstep();
                return;
            }
        }

        // Now continue along the wall.
        // First calculate remainder.
        scratch.bestslidefrac = FRACUNIT - (scratch.bestslidefrac + Fixed {0x800});

        if (scratch.bestslidefrac > FRACUNIT)
            scratch.bestslidefrac = FRACUNIT;

        if (!scratch.bestslidefrac.isPositive())
            return;

        scratch.tmmove.x = FixedMul(mom.x, scratch.bestslidefrac);
        scratch.tmmove.y = FixedMul(mom.y, scratch.bestslidefrac);

        hitSlideLine(scratch.bestslideline); // clip the moves

        mom.setXY(scratch.tmmove);

        // A successful move ends the slide; a blocked one retries the loop.
        if (tryMove({pos.x + scratch.tmmove.x, pos.y + scratch.tmmove.y}))
            return;
    }
}

//
// aimLineAttack
//
AimResult aimLineAttack(Mobj* t1, Angle angle, Fixed distance)
{
    auto& clip = clipping();

    const auto angleFine = angle.fineIndex();
    auto* shootthing = t1;

    // The range in WHOLE units scales the fixed cosine: an integer product, not a
    // fixed-point one. FixedMul here would divide the reach by 65536.
    auto x2 = t1->pos.x + distance.toInt() * finecosine()[angleFine];
    auto y2 = t1->pos.y + distance.toInt() * finesine()[angleFine];
    auto shootz = t1->pos.z + (t1->height >> 1) + 8 * FRACUNIT;

    // can't shoot outside view angles
    auto topslope = 100 * FRACUNIT / 160;
    auto bottomslope = -100 * FRACUNIT / 160;

    clip.attackrange = distance;

    AimResult result;

    const auto tryAim =
        [shootthing, shootz, &topslope, &bottomslope, &result](Intercept* in)
    { return aimTraverse(in, shootthing, shootz, topslope, bottomslope, result); };

    pathTraverse(t1->pos.xy(), {x2, y2}, PT_ADDLINES | PT_ADDTHINGS, tryAim);

    return result;
}

//
// lineAttack
// If damage == 0, it is just a test trace used only to find an aim target.
//
void Mobj::lineAttack(Angle angleToUse, Fixed distance, Fixed slope, int damage)
{
    auto& clip = clipping();

    const auto angleFine = angleToUse.fineIndex();
    auto* shootthing = this;
    auto la_damage = damage;
    // Whole units scaling the fixed cosine - an integer product. See aimLineAttack.
    auto x2 = pos.x + distance.toInt() * finecosine()[angleFine];
    auto y2 = pos.y + distance.toInt() * finesine()[angleFine];
    auto shootz = pos.z + (height >> 1) + 8 * FRACUNIT;
    clip.attackrange = distance;
    auto aimslope = slope;

    const auto tryShoot = [shootthing, shootz, aimslope, la_damage](Intercept* in)
    { return shootTraverse(in, shootthing, shootz, aimslope, la_damage); };

    pathTraverse(pos.xy(), {x2, y2}, PT_ADDLINES | PT_ADDTHINGS, tryShoot);
}

//
// useLines
// Looks for special lines in front of the player to activate.
//
void Player::useLines()
{
    auto angle = mo->angle;
    const auto angleFine = angle.fineIndex();

    auto x1 = mo->pos.x;
    auto y1 = mo->pos.y;
    // USERANGE in whole units scaling the fixed cosine - an integer product.
    auto x2 = x1 + USERANGE.toInt() * finecosine()[angleFine];
    auto y2 = y1 + USERANGE.toInt() * finesine()[angleFine];

    auto* usething = mo;

    const auto tryLine = [usething](Intercept* in)
    { return useTraverse(in, usething); };

    pathTraverse({x1, y1}, {x2, y2}, PT_ADDLINES, tryLine);
}

//
// radiusAttack
// Source is the creature that caused the explosion at spot.
//
void Mobj::radiusAttack(Mobj* source, int damage)
{
    // Vanilla, overflow and all: MAXRADIUS is already a fixed value, so shifting
    // (damage + MAXRADIUS) up by another fracBits wraps the MAXRADIUS term away
    // and leaves the damage alone as the radius. Kept as the integer expression
    // it has always been - the demos are recorded against the wrap.
    Fixed dist {(damage + (MAXRADIUS).raw) << fracBits};

    // Blockmap cell indices: a raw shift by MAPBLOCKSHIFT, not a conversion.
    auto yh = (pos.y + dist - level().blockmap.origin.y).raw >> MAPBLOCKSHIFT;
    auto yl = (pos.y - dist - level().blockmap.origin.y).raw >> MAPBLOCKSHIFT;
    auto xh = (pos.x + dist - level().blockmap.origin.x).raw >> MAPBLOCKSHIFT;
    auto xl = (pos.x - dist - level().blockmap.origin.x).raw >> MAPBLOCKSHIFT;
    const auto hitThing = [this, source, damage](Mobj* thing)
    { return radiusAttackThing(thing, this, source, damage); };

    for (auto by = yl; by <= yh; by++)
        for (auto bx = xl; bx <= xh; bx++)
            forEachThingInBlock(bx, by, hitThing);
}

//
// changeSector
//
bool Sector::changeSector(bool crunch)
{
    auto nofit = false;

    const auto clipThing = [crunch, &nofit](Mobj* thing)
    { return changeSectorThing(thing, crunch, nofit); };

    // re-check heights for all things near the moving sector
    for (auto x = blockbox[boxLeft]; x <= blockbox[boxRight]; x++)
        for (auto y = blockbox[boxBottom]; y <= blockbox[boxTop]; y++)
            forEachThingInBlock(x, y, clipThing);

    return nofit;
}
} // namespace Doom
