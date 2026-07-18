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
// This file is now a shim: the pure geometry lives in Sim/MapGeometry.h and the
// stateful iterators / linking / traversal in Sim/MapUtil.{h,cpp}, both real C++ in
// namespace Doom. The vanilla free-function names below forward there so the rest
// of the still-vanilla engine calls them unchanged.
//
//-----------------------------------------------------------------------------


#include "doom_config.h"



#include "m_bbox.h"
#include "doomdef.h"
#include "p_local.h"
#include "r_state.h" // State.

#include "Sim/Clip.h"
#include "Sim/MapGeometry.h"
#include "Sim/MapUtil.h"

// Reads a vanilla divline_t's four fields as a Doom::DivLine, so the geometry
// helpers can take a clean value while the traversal code still stores the vanilla
// struct.
static Doom::DivLine asDivLine(divline_t* d)
{
    return {{Doom::Fixed {d->x}, Doom::Fixed {d->y}},
            {Doom::Fixed {d->dx}, Doom::Fixed {d->dy}}};
}


// The clipping window and the trace live in Doom::Clip now; these vanilla names are
// references onto that owner, bound at static-init time through Doom::clip() the way
// m_random binds rndindex. p_map/p_sight/p_enemy still read them as globals.
fixed_t& opentop = Doom::clip().opentop;
fixed_t& openbottom = Doom::clip().openbottom;
fixed_t& openrange = Doom::clip().openrange;
fixed_t& lowfloor = Doom::clip().lowfloor;
divline_t& trace = Doom::clip().trace;


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
int P_PointOnLineSide(fixed_t x, fixed_t y, Doom::Line* line)
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
int P_BoxOnLineSide(fixed_t* tmbox, Doom::Line* ld)
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
void P_MakeDivline(Doom::Line* li, divline_t* dl)
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
void P_LineOpening(Doom::Line* linedef)
{
    Doom::Sector* front;
    Doom::Sector* back;

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
// THING POSITION SETTING, BLOCK MAP ITERATORS, INTERCEPT ROUTINES
// All rewritten in Sim/MapUtil.{h,cpp}; these keep the vanilla names.
//

void P_UnsetThingPosition(mobj_t* thing)
{
    Doom::unsetThingPosition(*thing);
}


void P_SetThingPosition(mobj_t* thing)
{
    Doom::setThingPosition(*thing);
}


doom_boolean P_BlockLinesIterator(int x, int y, doom_boolean (*func)(Doom::Line*))
{
    return Doom::forEachLineInBlock(x, y, func);
}


doom_boolean P_BlockThingsIterator(int x, int y, doom_boolean (*func)(mobj_t*))
{
    return Doom::forEachThingInBlock(x, y, func);
}


doom_boolean P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, int flags, doom_boolean(*trav) (intercept_t*))
{
    return Doom::pathTraverse(x1, y1, x2, y2, flags, trav);
}
