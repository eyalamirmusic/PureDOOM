#include "Movement.h"

#include "../Host/Platform.h" // doom_abs
#include "../Game/GameSession.h" // gamemap
#include "../Game/MapSpawns.h"
#include "Random.h"
#include "SimDefs.h"

#include "Clip.h"
#include "ValidCount.h"
#include "Level.h"
#include "MapUtil.h"

#include "Specials.h"
#include "../Render/Main.h"
#include "Interaction.h"
#include "Mobj.h"
#include "Random.h"
#include "../Math/BBox.h"
namespace Doom
{
namespace
{
// PIT_StompThing: telefrag a shootable thing occupying the teleport destination.
bool stompThing(Mobj* thing)
{
    Clip& clip = Doom::clip();

    if (!(hasFlag(thing->flags, MobjFlag::Shootable)))
        return true;

    fixed_t blockdist = thing->radius + clip.tmthing->radius;

    if (doom_abs(thing->x - clip.tmx) >= blockdist
        || doom_abs(thing->y - clip.tmy) >= blockdist)
    {
        // didn't hit it
        return true;
    }

    // don't clip against self
    if (thing == clip.tmthing)
        return true;

    // monsters don't stomp things except on boss level
    if (!clip.tmthing->player && gameSession().gamemap != 30)
        return false;

    damageMobj(*thing, clip.tmthing, clip.tmthing, 10000);

    return true;
}

// PIT_CheckLine: a line the mover's box crosses. Blocks the move (return false) if
// solid or explicitly blocking; otherwise narrows tmfloorz/tmceilingz/tmdropoffz to
// the opening it leaves and records a special line for later.
bool checkLine(Line* ld)
{
    Clip& clip = Doom::clip();

    if (clip.tmbbox[boxRight] <= ld->bbox[boxLeft]
        || clip.tmbbox[boxLeft] >= ld->bbox[boxRight]
        || clip.tmbbox[boxTop] <= ld->bbox[boxBottom]
        || clip.tmbbox[boxBottom] >= ld->bbox[boxTop])
        return true;

    if (boxLineSide(clip.tmbbox.data(), *ld) != -1)
        return true;

    // A line has been hit.
    //
    // The moving thing's destination position will cross the given line. If this
    // should not be allowed, return false. If the line is special, keep track of it
    // to process later if the move is proven ok. NOTE: specials are NOT sorted by
    // order, so two special lines only 8 pixels apart could be crossed in either
    // order.

    if (!ld->backsector)
        return false; // one sided line

    if (!(hasFlag(clip.tmthing->flags, MobjFlag::Missile)))
    {
        if (ld->flags & ML_BLOCKING)
            return false; // explicitly blocking everything

        if (!clip.tmthing->player && ld->flags & ML_BLOCKMONSTERS)
            return false; // block monsters only
    }

    // set openrange, opentop, openbottom
    updateLineOpening(*ld);

    // adjust floor / ceiling heights
    if (clip.opentop < clip.tmceilingz)
    {
        clip.tmceilingz = clip.opentop;
        clip.ceilingline = ld;
    }

    if (clip.openbottom > clip.tmfloorz)
        clip.tmfloorz = clip.openbottom;

    if (clip.lowfloor < clip.tmdropoffz)
        clip.tmdropoffz = clip.lowfloor;

    // if contacted a special line, add it to the list
    if (ld->special)
    {
        clip.spechit[clip.numspechit] = ld;
        clip.numspechit++;
    }

    return true;
}

// PIT_CheckThing: the mover against another thing - skull slam, missile impact,
// pickup, or a plain solid block.
bool checkThing(Mobj* thing)
{
    Clip& clip = Doom::clip();

    if (!(hasFlag(
            thing->flags, MobjFlag::Solid, MobjFlag::Special, MobjFlag::Shootable)))
        return true;

    fixed_t blockdist = thing->radius + clip.tmthing->radius;

    if (doom_abs(thing->x - clip.tmx) >= blockdist
        || doom_abs(thing->y - clip.tmy) >= blockdist)
    {
        // didn't hit it
        return true;
    }

    // don't clip against self
    if (thing == clip.tmthing)
        return true;

    // check for skulls slamming into things
    if (hasFlag(clip.tmthing->flags, MobjFlag::SkullFly))
    {
        int damage = ((randomness().forPlay() % 8) + 1) * clip.tmthing->info->damage;

        damageMobj(*thing, clip.tmthing, clip.tmthing, damage);

        clip.tmthing->flags = withoutFlags(clip.tmthing->flags, MobjFlag::SkullFly);
        clip.tmthing->momx = clip.tmthing->momy = clip.tmthing->momz = fixed_t {};

        setMobjState(*clip.tmthing,
                     static_cast<StateNum>(clip.tmthing->info->spawnstate));

        return false; // stop moving
    }

    // missiles can hit other things
    if (hasFlag(clip.tmthing->flags, MobjFlag::Missile))
    {
        // see if it went over / under
        if (clip.tmthing->z > thing->z + thing->height)
            return true; // overhead
        if (clip.tmthing->z + clip.tmthing->height < thing->z)
            return true; // underneath

        if (clip.tmthing->target
            && (clip.tmthing->target->type == thing->type
                || (clip.tmthing->target->type == MobjType::Knight
                    && thing->type == MobjType::Bruiser)
                || (clip.tmthing->target->type == MobjType::Bruiser
                    && thing->type == MobjType::Knight)))
        {
            // Don't hit same species as originator.
            if (thing == clip.tmthing->target)
                return true;

            if (thing->type != MobjType::Player)
            {
                // Explode, but do no damage.
                // Let players missile other players.
                return false;
            }
        }

        if (!(hasFlag(thing->flags, MobjFlag::Shootable)))
        {
            // didn't do any damage
            return !(hasFlag(thing->flags, MobjFlag::Solid));
        }

        // damage / explode
        int damage = ((randomness().forPlay() % 8) + 1) * clip.tmthing->info->damage;
        damageMobj(*thing, clip.tmthing, clip.tmthing->target, damage);

        // don't traverse any more
        return false;
    }

    // check for special pickup
    if (hasFlag(thing->flags, MobjFlag::Special))
    {
        bool solid = hasFlag(thing->flags, MobjFlag::Solid);
        if (hasFlag(clip.tmflags, MobjFlag::Pickup))
        {
            // can remove thing
            touchSpecialThing(*thing, *clip.tmthing);
        }
        return !solid;
    }

    return !(hasFlag(thing->flags, MobjFlag::Solid));
}
} // namespace

bool checkPosition(Mobj& thing, fixed_t x, fixed_t y)
{
    Clip& clip = Doom::clip();

    clip.tmthing = &thing;
    clip.tmflags = thing.flags;

    clip.tmx = x;
    clip.tmy = y;

    clip.tmbbox[boxTop] = y + clip.tmthing->radius;
    clip.tmbbox[boxBottom] = y - clip.tmthing->radius;
    clip.tmbbox[boxRight] = x + clip.tmthing->radius;
    clip.tmbbox[boxLeft] = x - clip.tmthing->radius;

    SubSector* newsubsec = pointInSubsector(x, y);
    clip.ceilingline = nullptr;

    // The base floor / ceiling is from the subsector that contains the point. Any
    // contacted lines the step closer together will adjust them.
    clip.tmfloorz = clip.tmdropoffz = newsubsec->sector->floorheight;
    clip.tmceilingz = newsubsec->sector->ceilingheight;

    validCount().validcount++;
    clip.numspechit = 0;

    if (hasFlag(clip.tmflags, MobjFlag::NoClip))
        return true;

    // Check things first, possibly picking things up. The bounding box is extended
    // by MAXRADIUS because mobj_ts are grouped into mapblocks based on their origin
    // point, and can overlap into adjacent blocks by up to MAXRADIUS units.
    const Blockmap& bmap = level().blockmap;

    int xl = bmap.blockX(clip.tmbbox[boxLeft] - MAXRADIUS);
    int xh = bmap.blockX(clip.tmbbox[boxRight] + MAXRADIUS);
    int yl = bmap.blockY(clip.tmbbox[boxBottom] - MAXRADIUS);
    int yh = bmap.blockY(clip.tmbbox[boxTop] + MAXRADIUS);

    for (int bx = xl; bx <= xh; bx++)
        for (int by = yl; by <= yh; by++)
            if (!forEachThingInBlock(bx, by, checkThing))
                return false;

    // check lines
    xl = bmap.blockX(clip.tmbbox[boxLeft]);
    xh = bmap.blockX(clip.tmbbox[boxRight]);
    yl = bmap.blockY(clip.tmbbox[boxBottom]);
    yh = bmap.blockY(clip.tmbbox[boxTop]);

    for (int bx = xl; bx <= xh; bx++)
        for (int by = yl; by <= yh; by++)
            if (!forEachLineInBlock(bx, by, checkLine))
                return false;

    return true;
}

bool tryMove(Mobj& thing, fixed_t x, fixed_t y)
{
    Clip& clip = Doom::clip();

    clip.floatok = false;
    if (!checkPosition(thing, x, y))
        return false; // solid wall or thing

    if (!(hasFlag(thing.flags, MobjFlag::NoClip)))
    {
        if (clip.tmceilingz - clip.tmfloorz < thing.height)
            return false; // doesn't fit

        clip.floatok = true;

        if (!(hasFlag(thing.flags, MobjFlag::Teleport))
            && clip.tmceilingz - thing.z < thing.height)
            return false; // mobj must lower itself to fit

        if (!(hasFlag(thing.flags, MobjFlag::Teleport))
            && clip.tmfloorz - thing.z > 24 * FRACUNIT)
            return false; // too big a step up

        if (!(hasFlag(thing.flags, MobjFlag::DropOff, MobjFlag::Float))
            && clip.tmfloorz - clip.tmdropoffz > 24 * FRACUNIT)
            return false; // don't stand over a dropoff
    }

    // the move is ok, so link the thing into its new position
    unsetThingPosition(thing);

    fixed_t oldx = thing.x;
    fixed_t oldy = thing.y;
    thing.floorz = clip.tmfloorz;
    thing.ceilingz = clip.tmceilingz;
    thing.x = x;
    thing.y = y;

    setThingPosition(thing);

    // if any special lines were hit, do the effect
    if (!(hasFlag(thing.flags, MobjFlag::Teleport, MobjFlag::NoClip)))
    {
        while (clip.numspechit--)
        {
            // see if the line was crossed
            Line* ld = clip.spechit[clip.numspechit];
            int side = lineSide({thing.x, thing.y}, *ld);
            int oldside = lineSide({oldx, oldy}, *ld);
            if (side != oldside)
            {
                if (ld->special)
                    crossSpecialLine(static_cast<int>(ld - lines), oldside, thing);
            }
        }
    }

    return true;
}

bool teleportMove(Mobj& thing, fixed_t x, fixed_t y)
{
    Clip& clip = Doom::clip();

    // kill anything occupying the position
    clip.tmthing = &thing;
    clip.tmflags = thing.flags;

    clip.tmx = x;
    clip.tmy = y;

    clip.tmbbox[boxTop] = y + clip.tmthing->radius;
    clip.tmbbox[boxBottom] = y - clip.tmthing->radius;
    clip.tmbbox[boxRight] = x + clip.tmthing->radius;
    clip.tmbbox[boxLeft] = x - clip.tmthing->radius;

    SubSector* newsubsec = pointInSubsector(x, y);
    clip.ceilingline = nullptr;

    // The base floor/ceiling is from the subsector that contains the point. Any
    // contacted lines the step closer together will adjust them.
    clip.tmfloorz = clip.tmdropoffz = newsubsec->sector->floorheight;
    clip.tmceilingz = newsubsec->sector->ceilingheight;

    validCount().validcount++;
    clip.numspechit = 0;

    // stomp on any things contacted. Blockmap cell indices: the shift is
    // MAPBLOCKSHIFT over the raw 16.16 bits, not a fixed-to-whole conversion.
    int xl = (clip.tmbbox[boxLeft] - bmaporgx - MAXRADIUS).raw >> MAPBLOCKSHIFT;
    int xh = (clip.tmbbox[boxRight] - bmaporgx + MAXRADIUS).raw >> MAPBLOCKSHIFT;
    int yl = (clip.tmbbox[boxBottom] - bmaporgy - MAXRADIUS).raw >> MAPBLOCKSHIFT;
    int yh = (clip.tmbbox[boxTop] - bmaporgy + MAXRADIUS).raw >> MAPBLOCKSHIFT;

    for (int bx = xl; bx <= xh; bx++)
        for (int by = yl; by <= yh; by++)
            if (!forEachThingInBlock(bx, by, stompThing))
                return false;

    // the move is ok, so link the thing into its new position
    unsetThingPosition(thing);

    thing.floorz = clip.tmfloorz;
    thing.ceilingz = clip.tmceilingz;
    thing.x = x;
    thing.y = y;

    setThingPosition(thing);

    return true;
}

bool thingHeightClip(Mobj& thing)
{
    Clip& clip = Doom::clip();

    bool onfloor = (thing.z == thing.floorz);

    checkPosition(thing, thing.x, thing.y);
    // what about stranding a monster partially off an edge?

    thing.floorz = clip.tmfloorz;
    thing.ceilingz = clip.tmceilingz;

    if (onfloor)
    {
        // walking monsters rise and fall with the floor
        thing.z = thing.floorz;
    }
    else
    {
        // don't adjust a floating monster unless forced to
        if (thing.z + thing.height > thing.ceilingz)
            thing.z = thing.ceilingz - thing.height;
    }

    if (thing.ceilingz - thing.floorz < thing.height)
        return false;

    return true;
}
} // namespace Doom
