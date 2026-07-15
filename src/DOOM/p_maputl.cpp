// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//        Movement/collision utility functions,
//        as used by function in p_map.c. 
//        BLOCKMAP Iterator functions,
//        and some PIT_* functions to use for iteration.
//
//-----------------------------------------------------------------------------


#include "doom_config.h"



#include "m_bbox.h"
#include "doomdef.h"
#include "p_local.h"
#include "r_state.h" // State.

#include "Sim/MapGeometry.h"

// Reads a vanilla divline_t's four fields as a Doom::DivLine, so the geometry
// helpers can take a clean value while the traversal code still stores the vanilla
// struct.
static Doom::DivLine asDivLine(divline_t* d)
{
    return {{Doom::Fixed {d->x}, Doom::Fixed {d->y}},
            {Doom::Fixed {d->dx}, Doom::Fixed {d->dy}}};
}


fixed_t opentop;
fixed_t openbottom;
fixed_t openrange;
fixed_t lowfloor;
intercept_t intercepts[MAXINTERCEPTS];
intercept_t* intercept_p;
divline_t trace;
doom_boolean earlyout;
int ptflags;


//
// P_AproxDistance
// Gives an estimation of distance (not exact)
//
fixed_t P_AproxDistance(fixed_t dx, fixed_t dy)
{
    return Doom::approxDistance(Doom::Fixed {dx}, Doom::Fixed {dy}).raw;
}


//
// P_PointOnLineSide
// Returns 0 or 1
//
int P_PointOnLineSide(fixed_t x, fixed_t y, line_t* line)
{
    return Doom::pointOnLineSide({Doom::Fixed {x}, Doom::Fixed {y}},
                                 {Doom::Fixed {line->v1->x}, Doom::Fixed {line->v1->y}},
                                 {Doom::Fixed {line->dx}, Doom::Fixed {line->dy}});
}


//
// P_BoxOnLineSide
// Considers the line to be infinite
// Returns side 0 or 1, -1 if box crosses the line.
//
int P_BoxOnLineSide(fixed_t* tmbox, line_t* ld)
{
    return Doom::boxOnLineSide(Doom::Fixed {tmbox[BOXTOP]},
                               Doom::Fixed {tmbox[BOXBOTTOM]},
                               Doom::Fixed {tmbox[BOXLEFT]},
                               Doom::Fixed {tmbox[BOXRIGHT]},
                               {Doom::Fixed {ld->v1->x}, Doom::Fixed {ld->v1->y}},
                               {Doom::Fixed {ld->dx}, Doom::Fixed {ld->dy}},
                               ld->slopetype);
}


//
// P_PointOnDivlineSide
// Returns 0 or 1.
//
int P_PointOnDivlineSide(fixed_t x, fixed_t y, divline_t* line)
{
    return Doom::pointOnDivlineSide({Doom::Fixed {x}, Doom::Fixed {y}},
                                    asDivLine(line));
}


//
// P_MakeDivline
//
void P_MakeDivline(line_t* li, divline_t* dl)
{
    dl->x = li->v1->x;
    dl->y = li->v1->y;
    dl->dx = li->dx;
    dl->dy = li->dy;
}


//
// P_InterceptVector
// Returns the fractional intercept point
// along the first divline.
// This is only called by the addthings
// and addlines traversers.
//
fixed_t P_InterceptVector(divline_t* v2, divline_t* v1)
{
    // interceptVector(a, b) computes the crossing along `a`; vanilla's v2 is that
    // `a` and v1 is `b`.
    return Doom::interceptVector(asDivLine(v2), asDivLine(v1)).raw;
}


//
// P_LineOpening
// Sets opentop and openbottom to the window
// through a two sided line.
// OPTIMIZE: keep this precalculated
//
void P_LineOpening(line_t* linedef)
{
    sector_t* front;
    sector_t* back;

    if (linedef->sidenum[1] == -1)
    {
        // single sided line
        openrange = 0;
        return;
    }

    front = linedef->frontsector;
    back = linedef->backsector;

    Doom::Opening opening = Doom::lineOpening(Doom::Fixed {front->floorheight},
                                              Doom::Fixed {front->ceilingheight},
                                              Doom::Fixed {back->floorheight},
                                              Doom::Fixed {back->ceilingheight});

    opentop = opening.top.raw;
    openbottom = opening.bottom.raw;
    lowfloor = opening.lowFloor.raw;
    openrange = opening.range.raw;
}


//
// THING POSITION SETTING
//

//
// P_UnsetThingPosition
// Unlinks a thing from block map and sectors.
// On each position change, BLOCKMAP and other
// lookups maintaining lists ot things inside
// these structures need to be updated.
//
void P_UnsetThingPosition(mobj_t* thing)
{
    int blockx;
    int blocky;

    if (!(thing->flags & MF_NOSECTOR))
    {
        // inert things don't need to be in blockmap?
        // unlink from subsector
        if (thing->snext)
            thing->snext->sprev = thing->sprev;

        if (thing->sprev)
            thing->sprev->snext = thing->snext;
        else
            thing->subsector->sector->thinglist = thing->snext;
    }

    if (!(thing->flags & MF_NOBLOCKMAP))
    {
        // inert things don't need to be in blockmap
        // unlink from block map
        if (thing->bnext)
            thing->bnext->bprev = thing->bprev;

        if (thing->bprev)
            thing->bprev->bnext = thing->bnext;
        else
        {
            blockx = (thing->x - bmaporgx) >> MAPBLOCKSHIFT;
            blocky = (thing->y - bmaporgy) >> MAPBLOCKSHIFT;

            if (blockx >= 0 && blockx < bmapwidth
                && blocky >= 0 && blocky < bmapheight)
            {
                blocklinks[blocky * bmapwidth + blockx] = thing->bnext;
            }
        }
    }
}


//
// P_SetThingPosition
// Links a thing into both a block and a subsector
// based on it's x y.
// Sets thing->subsector properly
//
void P_SetThingPosition(mobj_t* thing)
{
    subsector_t* ss;
    sector_t* sec;
    int blockx;
    int blocky;
    mobj_t** link;


    // link into subsector
    ss = R_PointInSubsector(thing->x, thing->y);
    thing->subsector = ss;

    if (!(thing->flags & MF_NOSECTOR))
    {
        // invisible things don't go into the sector links
        sec = ss->sector;

        thing->sprev = 0;
        thing->snext = sec->thinglist;

        if (sec->thinglist)
            sec->thinglist->sprev = thing;

        sec->thinglist = thing;
    }

    // link into blockmap
    if (!(thing->flags & MF_NOBLOCKMAP))
    {
        // inert things don't need to be in blockmap                
        blockx = (thing->x - bmaporgx) >> MAPBLOCKSHIFT;
        blocky = (thing->y - bmaporgy) >> MAPBLOCKSHIFT;

        if (blockx >= 0
            && blockx < bmapwidth
            && blocky >= 0
            && blocky < bmapheight)
        {
            link = &blocklinks[blocky * bmapwidth + blockx];
            thing->bprev = 0;
            thing->bnext = *link;
            if (*link)
                (*link)->bprev = thing;

            *link = thing;
        }
        else
        {
            // thing is off the map
            thing->bnext = thing->bprev = 0;
        }
    }
}


//
// BLOCK MAP ITERATORS
// For each line/thing in the given mapblock,
// call the passed PIT_* function.
// If the function returns false,
// exit with false without checking anything else.
//

//
// P_BlockLinesIterator
// The validcount flags are used to avoid checking lines
// that are marked in multiple mapblocks,
// so increment validcount before the first call
// to P_BlockLinesIterator, then make one or more calls
// to it.
//
doom_boolean P_BlockLinesIterator(int x, int y, doom_boolean(*func)(line_t*))
{
    int offset;
    short* list;
    line_t* ld;

    if (x < 0
        || y < 0
        || x >= bmapwidth
        || y >= bmapheight)
    {
        return true;
    }

    offset = y * bmapwidth + x;

    offset = *(blockmap + offset);

    for (list = blockmaplump + offset; *list != -1; list++)
    {
        ld = &lines[*list];

        if (ld->validcount == validcount)
            continue;         // line has already been checked

        ld->validcount = validcount;

        if (!func(ld))
            return false;
    }
    return true;        // everything was checked
}


//
// P_BlockThingsIterator
//
doom_boolean P_BlockThingsIterator(int x, int y, doom_boolean(*func)(mobj_t*))
{
    mobj_t* mobj;

    if (x < 0
        || y < 0
        || x >= bmapwidth
        || y >= bmapheight)
    {
        return true;
    }


    for (mobj = blocklinks[y * bmapwidth + x];
         mobj;
         mobj = mobj->bnext)
    {
        if (!func(mobj))
            return false;
    }
    return true;
}


//
// INTERCEPT ROUTINES
//

//
// PIT_AddLineIntercepts.
// Looks for lines in the given block
// that intercept the given trace
// to add to the intercepts list.
//
// A line is crossed if its endpoints
// are on opposite sides of the trace.
// Returns true if earlyout and a solid line hit.
//
doom_boolean PIT_AddLineIntercepts(line_t* ld)
{
    int s1;
    int s2;
    fixed_t frac;
    divline_t dl;

    // avoid precision problems with two routines
    if (trace.dx > FRACUNIT * 16
        || trace.dy > FRACUNIT * 16
        || trace.dx < -FRACUNIT * 16
        || trace.dy < -FRACUNIT * 16)
    {
        s1 = P_PointOnDivlineSide(ld->v1->x, ld->v1->y, &trace);
        s2 = P_PointOnDivlineSide(ld->v2->x, ld->v2->y, &trace);
    }
    else
    {
        s1 = P_PointOnLineSide(trace.x, trace.y, ld);
        s2 = P_PointOnLineSide(trace.x + trace.dx, trace.y + trace.dy, ld);
    }

    if (s1 == s2)
        return true; // line isn't crossed

    // hit the line
    P_MakeDivline(ld, &dl);
    frac = P_InterceptVector(&trace, &dl);

    if (frac < 0)
        return true; // behind source

    // try to early out the check
    if (earlyout
        && frac < FRACUNIT
        && !ld->backsector)
    {
        return false; // stop checking
    }

    intercept_p->frac = frac;
    intercept_p->isaline = true;
    intercept_p->d.line = ld;
    intercept_p++;

    return true; // continue
}


//
// PIT_AddThingIntercepts
//
doom_boolean PIT_AddThingIntercepts(mobj_t* thing)
{
    fixed_t x1;
    fixed_t y1;
    fixed_t x2;
    fixed_t y2;

    int s1;
    int s2;

    doom_boolean tracepositive;

    divline_t dl;

    fixed_t frac;

    tracepositive = (trace.dx ^ trace.dy) > 0;

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

    s1 = P_PointOnDivlineSide(x1, y1, &trace);
    s2 = P_PointOnDivlineSide(x2, y2, &trace);

    if (s1 == s2)
        return true; // line isn't crossed

    dl.x = x1;
    dl.y = y1;
    dl.dx = x2 - x1;
    dl.dy = y2 - y1;

    frac = P_InterceptVector(&trace, &dl);

    if (frac < 0)
        return true; // behind source

    intercept_p->frac = frac;
    intercept_p->isaline = false;
    intercept_p->d.thing = thing;
    intercept_p++;

    return true; // keep going
}


//
// P_TraverseIntercepts
// Returns true if the traverser function returns true
// for all lines.
// 
doom_boolean P_TraverseIntercepts(traverser_t func, fixed_t maxfrac)
{
    int count;
    fixed_t dist;
    intercept_t* scan;
    intercept_t* in;

    count = (int)(intercept_p - intercepts);

    in = 0;                        // shut up compiler warning

    while (count--)
    {
        dist = DOOM_MAXINT;
        for (scan = intercepts; scan < intercept_p; scan++)
        {
            if (scan->frac < dist)
            {
                dist = scan->frac;
                in = scan;
            }
        }

        if (dist > maxfrac)
            return true;        // checked everything in range                

#if 0  // UNUSED
        {
            // don't check these yet, there may be others inserted
            in = scan = intercepts;
            for (scan = intercepts; scan < intercept_p; scan++)
                if (scan->frac > maxfrac)
                    *in++ = *scan;
            intercept_p = in;
            return false;
        }
#endif

        if (!func(in))
            return false;        // don't bother going farther

        in->frac = DOOM_MAXINT;
    }

    return true;                // everything was traversed
}


//
// P_PathTraverse
// Traces a line from x1,y1 to x2,y2,
// calling the traverser function for each.
// Returns true if the traverser function returns true
// for all lines.
//
doom_boolean P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, int flags, doom_boolean(*trav) (intercept_t*))
{
    fixed_t xt1;
    fixed_t yt1;
    fixed_t xt2;
    fixed_t yt2;

    fixed_t xstep;
    fixed_t ystep;

    fixed_t partial;

    fixed_t xintercept;
    fixed_t yintercept;

    int mapx;
    int mapy;

    int mapxstep;
    int mapystep;

    int count;

    earlyout = flags & PT_EARLYOUT;

    validcount++;
    intercept_p = intercepts;

    if (((x1 - bmaporgx) & (MAPBLOCKSIZE - 1)) == 0)
        x1 += FRACUNIT;        // don't side exactly on a line

    if (((y1 - bmaporgy) & (MAPBLOCKSIZE - 1)) == 0)
        y1 += FRACUNIT;        // don't side exactly on a line

    trace.x = x1;
    trace.y = y1;
    trace.dx = x2 - x1;
    trace.dy = y2 - y1;

    x1 -= bmaporgx;
    y1 -= bmaporgy;
    xt1 = x1 >> MAPBLOCKSHIFT;
    yt1 = y1 >> MAPBLOCKSHIFT;

    x2 -= bmaporgx;
    y2 -= bmaporgy;
    xt2 = x2 >> MAPBLOCKSHIFT;
    yt2 = y2 >> MAPBLOCKSHIFT;

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

    yintercept = (y1 >> MAPBTOFRAC) + FixedMul(partial, ystep);


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
    xintercept = (x1 >> MAPBTOFRAC) + FixedMul(partial, xstep);

    // Step through map blocks.
    // Count is present to prevent a round off error
    // from skipping the break.
    mapx = xt1;
    mapy = yt1;

    for (count = 0; count < 64; count++)
    {
        if (flags & PT_ADDLINES)
        {
            if (!P_BlockLinesIterator(mapx, mapy, PIT_AddLineIntercepts))
                return false;        // early out
        }

        if (flags & PT_ADDTHINGS)
        {
            if (!P_BlockThingsIterator(mapx, mapy, PIT_AddThingIntercepts))
                return false;        // early out
        }

        if (mapx == xt2
            && mapy == yt2)
        {
            break;
        }

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
    return P_TraverseIntercepts(trav, FRACUNIT);
}
