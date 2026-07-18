#include "MapUtil.h"

#include "MapGeometry.h"

#include "../m_bbox.h" // BOXTOP, BOXBOTTOM, BOXLEFT, BOXRIGHT

#include "Clip.h"
#include "../Render/Main.h"
namespace Doom
{
DivLine makeDivLine(const Line& line)
{
    return {{Fixed {line.v1->x}, Fixed {line.v1->y}},
            {Fixed {line.dx}, Fixed {line.dy}}};
}

int lineSide(Vec2 point, const Line& line)
{
    return pointOnLineSide(point,
                           {Fixed {line.v1->x}, Fixed {line.v1->y}},
                           {Fixed {line.dx}, Fixed {line.dy}});
}

int boxLineSide(const fixed_t* box, const Line& line)
{
    return boxOnLineSide(Fixed {box[BOXTOP]},
                         Fixed {box[BOXBOTTOM]},
                         Fixed {box[BOXLEFT]},
                         Fixed {box[BOXRIGHT]},
                         {Fixed {line.v1->x}, Fixed {line.v1->y}},
                         {Fixed {line.dx}, Fixed {line.dy}},
                         line.slopetype);
}

void updateLineOpening(const Line& linedef)
{
    Clip& clip = Doom::clip();

    if (linedef.sidenum[1] == -1)
    {
        // single sided line
        clip.openrange = 0;
        return;
    }

    const Sector& front = *linedef.frontsector;
    const Sector& back = *linedef.backsector;

    const Opening opening = lineOpening(Fixed {front.floorheight},
                                        Fixed {front.ceilingheight},
                                        Fixed {back.floorheight},
                                        Fixed {back.ceilingheight});

    clip.opentop = opening.top.raw;
    clip.openbottom = opening.bottom.raw;
    clip.lowfloor = opening.lowFloor.raw;
    clip.openrange = opening.range.raw;
}

namespace
{
// PIT_AddLineIntercepts: a line the trace crosses is added to the intercept list.
// A line is crossed when its two endpoints fall on opposite sides of the trace.
doom_boolean addLineIntercept(Line* ld)
{
    Clip& clip = Doom::clip();
    const DivLine& trace = clip.trace;

    int s1;
    int s2;

    // avoid precision problems with two routines
    if (trace.delta.x.raw > FRACUNIT * 16 || trace.delta.y.raw > FRACUNIT * 16
        || trace.delta.x.raw < -FRACUNIT * 16 || trace.delta.y.raw < -FRACUNIT * 16)
    {
        s1 = pointOnDivlineSide({Fixed {ld->v1->x}, Fixed {ld->v1->y}}, trace);
        s2 = pointOnDivlineSide({Fixed {ld->v2->x}, Fixed {ld->v2->y}}, trace);
    }
    else
    {
        s1 = lineSide(trace.origin, *ld);
        s2 = lineSide(trace.origin + trace.delta, *ld);
    }

    if (s1 == s2)
        return true; // line isn't crossed

    // hit the line
    DivLine dl = makeDivLine(*ld);
    fixed_t frac = interceptVector(trace, dl).raw;

    if (frac < 0)
        return true; // behind source

    // try to early out the check: a solid line before the source is a wall
    if (clip.earlyOut && frac < FRACUNIT && !ld->backsector)
        return false; // stop checking

    clip.interceptPtr->frac = frac;
    clip.interceptPtr->isaline = true;
    clip.interceptPtr->d.line = ld;
    clip.interceptPtr++;

    return true; // continue
}

// PIT_AddThingIntercepts: a thing whose bounding-box diagonal the trace crosses is
// added. The diagonal chosen is the one facing the trace, so a corner clip counts.
doom_boolean addThingIntercept(Mobj* thing)
{
    Clip& clip = Doom::clip();
    const DivLine& trace = clip.trace;

    doom_boolean tracepositive = (trace.delta.x.raw ^ trace.delta.y.raw) > 0;

    fixed_t x1;
    fixed_t y1;
    fixed_t x2;
    fixed_t y2;

    // check a corner to corner crossection for hit
    if (tracepositive)
    {
        x1 = thing->x - thing->radius;
        y1 = thing->y + thing->radius;

        x2 = thing->x + thing->radius;
        y2 = thing->y - thing->radius;
    }
    else
    {
        x1 = thing->x - thing->radius;
        y1 = thing->y - thing->radius;

        x2 = thing->x + thing->radius;
        y2 = thing->y + thing->radius;
    }

    int s1 = pointOnDivlineSide({Fixed {x1}, Fixed {y1}}, trace);
    int s2 = pointOnDivlineSide({Fixed {x2}, Fixed {y2}}, trace);

    if (s1 == s2)
        return true; // line isn't crossed

    const DivLine dl {{Fixed {x1}, Fixed {y1}}, {Fixed {x2 - x1}, Fixed {y2 - y1}}};

    fixed_t frac = interceptVector(trace, dl).raw;

    if (frac < 0)
        return true; // behind source

    clip.interceptPtr->frac = frac;
    clip.interceptPtr->isaline = false;
    clip.interceptPtr->d.thing = thing;
    clip.interceptPtr++;

    return true; // keep going
}

// P_TraverseIntercepts: hand the gathered intercepts to func from nearest to
// farthest, stopping if func returns false or the next one is past maxfrac.
bool traverseIntercepts(Traverser func, fixed_t maxfrac)
{
    Clip& clip = Doom::clip();

    int count = static_cast<int>(clip.interceptPtr - clip.intercepts);

    Intercept* in = nullptr; // shut up compiler warning

    while (count--)
    {
        fixed_t dist = DOOM_MAXINT;

        for (Intercept* scan = clip.intercepts; scan < clip.interceptPtr; scan++)
        {
            if (scan->frac < dist)
            {
                dist = scan->frac;
                in = scan;
            }
        }

        if (dist > maxfrac)
            return true; // checked everything in range

        if (!func(in))
            return false; // don't bother going farther

        in->frac = DOOM_MAXINT;
    }

    return true; // everything was traversed
}
} // namespace

void setThingPosition(Mobj& thing)
{
    // link into subsector
    SubSector* ss = Doom::pointInSubsector(thing.x, thing.y);
    thing.subsector = ss;

    if (!(thing.flags & MF_NOSECTOR))
    {
        // invisible things don't go into the sector links
        Sector* sec = ss->sector;

        thing.sprev = nullptr;
        thing.snext = sec->thinglist;

        if (sec->thinglist)
            sec->thinglist->sprev = &thing;

        sec->thinglist = &thing;
    }

    // link into blockmap
    if (!(thing.flags & MF_NOBLOCKMAP))
    {
        // inert things don't need to be in the blockmap
        const Blockmap& bmap = level().blockmap;
        int blockx = bmap.blockX(Fixed {thing.x});
        int blocky = bmap.blockY(Fixed {thing.y});

        if (bmap.contains(blockx, blocky))
        {
            Mobj** link = &blocklinks[bmap.index(blockx, blocky)];
            thing.bprev = nullptr;
            thing.bnext = *link;
            if (*link)
                (*link)->bprev = &thing;

            *link = &thing;
        }
        else
        {
            // thing is off the map
            thing.bnext = thing.bprev = nullptr;
        }
    }
}

void unsetThingPosition(Mobj& thing)
{
    if (!(thing.flags & MF_NOSECTOR))
    {
        // inert things don't need to be in the blockmap?
        // unlink from subsector
        if (thing.snext)
            thing.snext->sprev = thing.sprev;

        if (thing.sprev)
            thing.sprev->snext = thing.snext;
        else
            thing.subsector->sector->thinglist = thing.snext;
    }

    if (!(thing.flags & MF_NOBLOCKMAP))
    {
        // unlink from the blockmap
        if (thing.bnext)
            thing.bnext->bprev = thing.bprev;

        if (thing.bprev)
            thing.bprev->bnext = thing.bnext;
        else
        {
            const Blockmap& bmap = level().blockmap;
            int blockx = bmap.blockX(Fixed {thing.x});
            int blocky = bmap.blockY(Fixed {thing.y});

            if (bmap.contains(blockx, blocky))
                blocklinks[bmap.index(blockx, blocky)] = thing.bnext;
        }
    }
}

bool pathTraverse(
    fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, int flags, Traverser trav)
{
    Clip& clip = Doom::clip();

    clip.earlyOut = flags & PT_EARLYOUT;

    validcount++;
    clip.interceptPtr = clip.intercepts;

    if (((x1 - bmaporgx) & (MAPBLOCKSIZE - 1)) == 0)
        x1 += FRACUNIT; // don't side exactly on a line

    if (((y1 - bmaporgy) & (MAPBLOCKSIZE - 1)) == 0)
        y1 += FRACUNIT; // don't side exactly on a line

    clip.trace.origin = {Fixed {x1}, Fixed {y1}};
    clip.trace.delta = {Fixed {x2 - x1}, Fixed {y2 - y1}};

    x1 -= bmaporgx;
    y1 -= bmaporgy;
    fixed_t xt1 = x1 >> MAPBLOCKSHIFT;
    fixed_t yt1 = y1 >> MAPBLOCKSHIFT;

    x2 -= bmaporgx;
    y2 -= bmaporgy;
    fixed_t xt2 = x2 >> MAPBLOCKSHIFT;
    fixed_t yt2 = y2 >> MAPBLOCKSHIFT;

    fixed_t partial;
    fixed_t xstep;
    fixed_t ystep;
    int mapxstep;
    int mapystep;

    if (xt2 > xt1)
    {
        mapxstep = 1;
        partial = FRACUNIT - ((x1 >> MAPBTOFRAC) & (FRACUNIT - 1));
        ystep = FixedDiv(y2 - y1, doom_abs(x2 - x1));
    }
    else if (xt2 < xt1)
    {
        mapxstep = -1;
        partial = (x1 >> MAPBTOFRAC) & (FRACUNIT - 1);
        ystep = FixedDiv(y2 - y1, doom_abs(x2 - x1));
    }
    else
    {
        mapxstep = 0;
        partial = FRACUNIT;
        ystep = 256 * FRACUNIT;
    }

    fixed_t yintercept = (y1 >> MAPBTOFRAC) + FixedMul(partial, ystep);

    if (yt2 > yt1)
    {
        mapystep = 1;
        partial = FRACUNIT - ((y1 >> MAPBTOFRAC) & (FRACUNIT - 1));
        xstep = FixedDiv(x2 - x1, doom_abs(y2 - y1));
    }
    else if (yt2 < yt1)
    {
        mapystep = -1;
        partial = (y1 >> MAPBTOFRAC) & (FRACUNIT - 1);
        xstep = FixedDiv(x2 - x1, doom_abs(y2 - y1));
    }
    else
    {
        mapystep = 0;
        partial = FRACUNIT;
        xstep = 256 * FRACUNIT;
    }

    fixed_t xintercept = (x1 >> MAPBTOFRAC) + FixedMul(partial, xstep);

    // Step through map blocks. Count is present to prevent a round off error from
    // skipping the break.
    int mapx = xt1;
    int mapy = yt1;

    for (int count = 0; count < 64; count++)
    {
        if (flags & PT_ADDLINES)
        {
            if (!forEachLineInBlock(mapx, mapy, addLineIntercept))
                return false; // early out
        }

        if (flags & PT_ADDTHINGS)
        {
            if (!forEachThingInBlock(mapx, mapy, addThingIntercept))
                return false; // early out
        }

        if (mapx == xt2 && mapy == yt2)
            break;

        if ((yintercept >> FRACBITS) == mapy)
        {
            yintercept += ystep;
            mapx += mapxstep;
        }
        else if ((xintercept >> FRACBITS) == mapx)
        {
            xintercept += xstep;
            mapy += mapystep;
        }
    }

    // go through the sorted list
    return traverseIntercepts(trav, FRACUNIT);
}
} // namespace Doom
