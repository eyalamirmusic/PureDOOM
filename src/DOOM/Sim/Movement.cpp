#include "Movement.h"

#include "../doom_config.h" // doom_abs
#include "../doomstat.h" // gamemap
#include "../m_bbox.h"
#include "../m_random.h"
#include "../p_local.h"
#include "../r_state.h" // validcount, lines

#include "Clip.h"
#include "Level.h"
#include "MapUtil.h"

namespace Doom
{
namespace
{
// PIT_StompThing: telefrag a shootable thing occupying the teleport destination.
doom_boolean stompThing(mobj_t* thing)
{
    Clip& clip = Doom::clip();

    if (!(thing->flags & MF_SHOOTABLE))
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
    if (!clip.tmthing->player && gamemap != 30)
        return false;

    P_DamageMobj(thing, clip.tmthing, clip.tmthing, 10000);

    return true;
}

// PIT_CheckLine: a line the mover's box crosses. Blocks the move (return false) if
// solid or explicitly blocking; otherwise narrows tmfloorz/tmceilingz/tmdropoffz to
// the opening it leaves and records a special line for later.
doom_boolean checkLine(line_t* ld)
{
    Clip& clip = Doom::clip();

    if (clip.tmbbox[BOXRIGHT] <= ld->bbox[BOXLEFT]
        || clip.tmbbox[BOXLEFT] >= ld->bbox[BOXRIGHT]
        || clip.tmbbox[BOXTOP] <= ld->bbox[BOXBOTTOM]
        || clip.tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
        return true;

    if (P_BoxOnLineSide(clip.tmbbox, ld) != -1)
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

    if (!(clip.tmthing->flags & MF_MISSILE))
    {
        if (ld->flags & ML_BLOCKING)
            return false; // explicitly blocking everything

        if (!clip.tmthing->player && ld->flags & ML_BLOCKMONSTERS)
            return false; // block monsters only
    }

    // set openrange, opentop, openbottom
    P_LineOpening(ld);

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
doom_boolean checkThing(mobj_t* thing)
{
    Clip& clip = Doom::clip();

    if (!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)))
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
    if (clip.tmthing->flags & MF_SKULLFLY)
    {
        int damage = ((P_Random() % 8) + 1) * clip.tmthing->info->damage;

        P_DamageMobj(thing, clip.tmthing, clip.tmthing, damage);

        clip.tmthing->flags &= ~MF_SKULLFLY;
        clip.tmthing->momx = clip.tmthing->momy = clip.tmthing->momz = 0;

        P_SetMobjState(clip.tmthing, (statenum_t) (clip.tmthing->info->spawnstate));

        return false; // stop moving
    }

    // missiles can hit other things
    if (clip.tmthing->flags & MF_MISSILE)
    {
        // see if it went over / under
        if (clip.tmthing->z > thing->z + thing->height)
            return true; // overhead
        if (clip.tmthing->z + clip.tmthing->height < thing->z)
            return true; // underneath

        if (clip.tmthing->target
            && (clip.tmthing->target->type == thing->type
                || (clip.tmthing->target->type == MT_KNIGHT
                    && thing->type == MT_BRUISER)
                || (clip.tmthing->target->type == MT_BRUISER
                    && thing->type == MT_KNIGHT)))
        {
            // Don't hit same species as originator.
            if (thing == clip.tmthing->target)
                return true;

            if (thing->type != MT_PLAYER)
            {
                // Explode, but do no damage.
                // Let players missile other players.
                return false;
            }
        }

        if (!(thing->flags & MF_SHOOTABLE))
        {
            // didn't do any damage
            return !(thing->flags & MF_SOLID);
        }

        // damage / explode
        int damage = ((P_Random() % 8) + 1) * clip.tmthing->info->damage;
        P_DamageMobj(thing, clip.tmthing, clip.tmthing->target, damage);

        // don't traverse any more
        return false;
    }

    // check for special pickup
    if (thing->flags & MF_SPECIAL)
    {
        doom_boolean solid = thing->flags & MF_SOLID;
        if (clip.tmflags & MF_PICKUP)
        {
            // can remove thing
            P_TouchSpecialThing(thing, clip.tmthing);
        }
        return !solid;
    }

    return !(thing->flags & MF_SOLID);
}
} // namespace

bool checkPosition(mobj_t* thing, fixed_t x, fixed_t y)
{
    Clip& clip = Doom::clip();

    clip.tmthing = thing;
    clip.tmflags = thing->flags;

    clip.tmx = x;
    clip.tmy = y;

    clip.tmbbox[BOXTOP] = y + clip.tmthing->radius;
    clip.tmbbox[BOXBOTTOM] = y - clip.tmthing->radius;
    clip.tmbbox[BOXRIGHT] = x + clip.tmthing->radius;
    clip.tmbbox[BOXLEFT] = x - clip.tmthing->radius;

    subsector_t* newsubsec = R_PointInSubsector(x, y);
    clip.ceilingline = nullptr;

    // The base floor / ceiling is from the subsector that contains the point. Any
    // contacted lines the step closer together will adjust them.
    clip.tmfloorz = clip.tmdropoffz = newsubsec->sector->floorheight;
    clip.tmceilingz = newsubsec->sector->ceilingheight;

    validcount++;
    clip.numspechit = 0;

    if (clip.tmflags & MF_NOCLIP)
        return true;

    // Check things first, possibly picking things up. The bounding box is extended
    // by MAXRADIUS because mobj_ts are grouped into mapblocks based on their origin
    // point, and can overlap into adjacent blocks by up to MAXRADIUS units.
    const Blockmap& bmap = level().blockmap;

    int xl = bmap.blockX(Fixed {clip.tmbbox[BOXLEFT] - MAXRADIUS});
    int xh = bmap.blockX(Fixed {clip.tmbbox[BOXRIGHT] + MAXRADIUS});
    int yl = bmap.blockY(Fixed {clip.tmbbox[BOXBOTTOM] - MAXRADIUS});
    int yh = bmap.blockY(Fixed {clip.tmbbox[BOXTOP] + MAXRADIUS});

    for (int bx = xl; bx <= xh; bx++)
        for (int by = yl; by <= yh; by++)
            if (!forEachThingInBlock(bx, by, checkThing))
                return false;

    // check lines
    xl = bmap.blockX(Fixed {clip.tmbbox[BOXLEFT]});
    xh = bmap.blockX(Fixed {clip.tmbbox[BOXRIGHT]});
    yl = bmap.blockY(Fixed {clip.tmbbox[BOXBOTTOM]});
    yh = bmap.blockY(Fixed {clip.tmbbox[BOXTOP]});

    for (int bx = xl; bx <= xh; bx++)
        for (int by = yl; by <= yh; by++)
            if (!forEachLineInBlock(bx, by, checkLine))
                return false;

    return true;
}

bool tryMove(mobj_t* thing, fixed_t x, fixed_t y)
{
    Clip& clip = Doom::clip();

    clip.floatok = false;
    if (!checkPosition(thing, x, y))
        return false; // solid wall or thing

    if (!(thing->flags & MF_NOCLIP))
    {
        if (clip.tmceilingz - clip.tmfloorz < thing->height)
            return false; // doesn't fit

        clip.floatok = true;

        if (!(thing->flags & MF_TELEPORT)
            && clip.tmceilingz - thing->z < thing->height)
            return false; // mobj must lower itself to fit

        if (!(thing->flags & MF_TELEPORT)
            && clip.tmfloorz - thing->z > 24 * FRACUNIT)
            return false; // too big a step up

        if (!(thing->flags & (MF_DROPOFF | MF_FLOAT))
            && clip.tmfloorz - clip.tmdropoffz > 24 * FRACUNIT)
            return false; // don't stand over a dropoff
    }

    // the move is ok, so link the thing into its new position
    P_UnsetThingPosition(thing);

    fixed_t oldx = thing->x;
    fixed_t oldy = thing->y;
    thing->floorz = clip.tmfloorz;
    thing->ceilingz = clip.tmceilingz;
    thing->x = x;
    thing->y = y;

    P_SetThingPosition(thing);

    // if any special lines were hit, do the effect
    if (!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
    {
        while (clip.numspechit--)
        {
            // see if the line was crossed
            line_t* ld = clip.spechit[clip.numspechit];
            int side = P_PointOnLineSide(thing->x, thing->y, ld);
            int oldside = P_PointOnLineSide(oldx, oldy, ld);
            if (side != oldside)
            {
                if (ld->special)
                    P_CrossSpecialLine((int) (ld - lines), oldside, thing);
            }
        }
    }

    return true;
}

bool teleportMove(mobj_t* thing, fixed_t x, fixed_t y)
{
    Clip& clip = Doom::clip();

    // kill anything occupying the position
    clip.tmthing = thing;
    clip.tmflags = thing->flags;

    clip.tmx = x;
    clip.tmy = y;

    clip.tmbbox[BOXTOP] = y + clip.tmthing->radius;
    clip.tmbbox[BOXBOTTOM] = y - clip.tmthing->radius;
    clip.tmbbox[BOXRIGHT] = x + clip.tmthing->radius;
    clip.tmbbox[BOXLEFT] = x - clip.tmthing->radius;

    subsector_t* newsubsec = R_PointInSubsector(x, y);
    clip.ceilingline = nullptr;

    // The base floor/ceiling is from the subsector that contains the point. Any
    // contacted lines the step closer together will adjust them.
    clip.tmfloorz = clip.tmdropoffz = newsubsec->sector->floorheight;
    clip.tmceilingz = newsubsec->sector->ceilingheight;

    validcount++;
    clip.numspechit = 0;

    // stomp on any things contacted
    int xl = (clip.tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
    int xh = (clip.tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
    int yl = (clip.tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
    int yh = (clip.tmbbox[BOXTOP] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

    for (int bx = xl; bx <= xh; bx++)
        for (int by = yl; by <= yh; by++)
            if (!forEachThingInBlock(bx, by, stompThing))
                return false;

    // the move is ok, so link the thing into its new position
    P_UnsetThingPosition(thing);

    thing->floorz = clip.tmfloorz;
    thing->ceilingz = clip.tmceilingz;
    thing->x = x;
    thing->y = y;

    P_SetThingPosition(thing);

    return true;
}

bool thingHeightClip(mobj_t* thing)
{
    Clip& clip = Doom::clip();

    doom_boolean onfloor = (thing->z == thing->floorz);

    checkPosition(thing, thing->x, thing->y);
    // what about stranding a monster partially off an edge?

    thing->floorz = clip.tmfloorz;
    thing->ceilingz = clip.tmceilingz;

    if (onfloor)
    {
        // walking monsters rise and fall with the floor
        thing->z = thing->floorz;
    }
    else
    {
        // don't adjust a floating monster unless forced to
        if (thing->z + thing->height > thing->ceilingz)
            thing->z = thing->ceilingz - thing->height;
    }

    if (thing->ceilingz - thing->floorz < thing->height)
        return false;

    return true;
}
} // namespace Doom
