// Rewritten out of vanilla r_bsp into namespace Doom.
//
// BSP traversal: walk the tree front-to-back for the view point, clip each
// subsector's segs against the solid-wall ranges, and hand visible wall ranges to
// r_segs and floor/ceiling to r_plane. r_bsp.cpp shims the R_ names and owns the
// drawseg list and clip ranges other renderer files read. Golden-neutral.

#include "../doom_config.h"

#include "../doomdef.h"
#include "../doomstat.h"
#include "../i_system.h"
#include "../m_bbox.h"
#include "../r_local.h"

#include "BSP.h"
#include "SolidSegs.h"

#include "Planes.h"
#include "Things.h"
#include <ea_data_structures/Structures/Array.h>

// Doom::storeWallRange lives in r_segs; declared so the BSP walk can hand it ranges.
#include "Segs.h"
void Doom::storeWallRange(int start, int stop);

#define MAXSEGS 32

namespace Doom
{
// cliprange_t and the solidsegs/newend clip ranges now live on the Engine (Render/SolidSegs.h, moved
// by the file-scope-statics sweep - REFACTOR.md, Step 5); the vanilla names are references onto that
// member. The type moved to the header so solidsegs could become a member (an anonymous-struct
// typedef in the .cpp cannot be named there).

// newend is one past the last valid seg
static cliprange_t*& newend = solidSegs().newend;

static cliprange_t (&solidsegs)[MAXSEGS] = solidSegs().solidsegs;

EA::Array<EA::Array<int, 4>, 12> checkcoord = {{3, 0, 2, 1},
                         {3, 0, 2, 0},
                         {3, 1, 2, 0},
                         {0},
                         {2, 0, 2, 1},
                         {0, 0, 0, 0},
                         {3, 1, 3, 0},
                         {0},
                         {2, 0, 3, 1},
                         {2, 1, 3, 1},
                         {2, 1, 3, 0}};

// Forward declarations so call order needs no rearranging.
void clearDrawSegs();
void clipSolidWallSegment(int first, int last);
void clipPassWallSegment(int first, int last);
void clearClipSegs();
void addLine(seg_t* line);
doom_boolean checkBBox(fixed_t* bspcoord);
void subsector(int num);
void renderBSPNode(int bspnum);

void clearDrawSegs()
{
    ds_p = drawsegs;
}

//
// clipSolidWallSegment
// Does handle solid walls,
//  e.g. single sided LineDefs (middle texture)
//  that entirely block the view.
//
void clipSolidWallSegment(int first, int last)
{
    cliprange_t* next;
    cliprange_t* start;

    // Find the first range that touches the range
    //  (adjacent pixels are touching).
    start = solidsegs;
    while (start->last < first - 1)
        start++;

    if (first < start->first)
    {
        if (last < start->first - 1)
        {
            // Post is entirely visible (above start),
            //  so insert a new clippost.
            Doom::storeWallRange(first, last);
            next = newend;
            newend++;

            while (next != start)
            {
                *next = *(next - 1);
                next--;
            }
            next->first = first;
            next->last = last;
            return;
        }

        // There is a fragment above *start.
        Doom::storeWallRange(first, start->first - 1);
        // Now adjust the clip size.
        start->first = first;
    }

    // Bottom contained in start?
    if (last <= start->last)
        return;

    next = start;
    while (last >= (next + 1)->first - 1)
    {
        // There is a fragment between two posts.
        Doom::storeWallRange(next->last + 1, (next + 1)->first - 1);
        next++;

        if (last <= next->last)
        {
            // Bottom is contained in next.
            // Adjust the clip size.
            start->last = next->last;
            goto crunch;
        }
    }

    // There is a fragment after *next.
    Doom::storeWallRange(next->last + 1, last);
    // Adjust the clip size.
    start->last = last;

    // Remove start+1 to next from the clip list,
    // because start now covers their area.
crunch:
    if (next == start)
    {
        // Post just extended past the bottom of one post.
        return;
    }

    while (next++ != newend)
    {
        // Remove a post.
        *++start = *next;
    }

    newend = start + 1;
}

//
// clipPassWallSegment
// Clips the given range of columns,
//  but does not includes it in the clip list.
// Does handle windows,
//  e.g. LineDefs with upper and lower texture.
//
void clipPassWallSegment(int first, int last)
{
    cliprange_t* start;

    // Find the first range that touches the range
    //  (adjacent pixels are touching).
    start = solidsegs;
    while (start->last < first - 1)
        start++;

    if (first < start->first)
    {
        if (last < start->first - 1)
        {
            // Post is entirely visible (above start).
            Doom::storeWallRange(first, last);
            return;
        }

        // There is a fragment above *start.
        Doom::storeWallRange(first, start->first - 1);
    }

    // Bottom contained in start?
    if (last <= start->last)
        return;

    while (last >= (start + 1)->first - 1)
    {
        // There is a fragment between two posts.
        Doom::storeWallRange(start->last + 1, (start + 1)->first - 1);
        start++;

        if (last <= start->last)
            return;
    }

    // There is a fragment after *next.
    Doom::storeWallRange(start->last + 1, last);
}

//
// clearClipSegs
//
void clearClipSegs()
{
    solidsegs[0].first = -0x7fffffff;
    solidsegs[0].last = -1;
    solidsegs[1].first = viewwidth;
    solidsegs[1].last = 0x7fffffff;
    newend = solidsegs + 2;
}

//
// addLine
// Clips the given segment
// and adds any visible pieces to the line list.
//
void addLine(seg_t* line)
{
    int x1;
    int x2;
    angle_t angle1;
    angle_t angle2;
    angle_t span;
    angle_t tspan;

    curline = line;

    // OPTIMIZE: quickly reject orthogonal back sides.
    angle1 = R_PointToAngle(line->v1->x, line->v1->y);
    angle2 = R_PointToAngle(line->v2->x, line->v2->y);

    // Clip to view edges.
    // OPTIMIZE: make constant out of 2*clipangle (FIELDOFVIEW).
    span = angle1 - angle2;

    // Back side? I.e. backface culling?
    if (span >= ANG180)
        return;

    // Global angle needed by segcalc.
    rw_angle1 = angle1;
    angle1 -= viewangle;
    angle2 -= viewangle;

    tspan = angle1 + clipangle;
    if (tspan > 2 * clipangle)
    {
        tspan -= 2 * clipangle;

        // Totally off the left edge?
        if (tspan >= span)
            return;

        angle1 = clipangle;
    }
    tspan = clipangle - angle2;
    if (tspan > 2 * clipangle)
    {
        tspan -= 2 * clipangle;

        // Totally off the left edge?
        if (tspan >= span)
            return;
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4146)
#endif
        angle2 = -clipangle;
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    }

    // The seg is in the view range,
    // but not necessarily visible.
    angle1 = (angle1 + ANG90) >> ANGLETOFINESHIFT;
    angle2 = (angle2 + ANG90) >> ANGLETOFINESHIFT;
    x1 = viewangletox[angle1];
    x2 = viewangletox[angle2];

    // Does not cross a pixel?
    if (x1 == x2)
        return;

    backsector = line->backsector;

    // Single sided line?
    if (!backsector)
        goto clipsolid;

    // Closed door.
    if (backsector->ceilingheight <= frontsector->floorheight
        || backsector->floorheight >= frontsector->ceilingheight)
        goto clipsolid;

    // Window.
    if (backsector->ceilingheight != frontsector->ceilingheight
        || backsector->floorheight != frontsector->floorheight)
        goto clippass;

    // Reject empty lines used for triggers
    //  and special events.
    // Identical floor and ceiling on both sides,
    // identical light levels on both sides,
    // and no middle texture.
    if (backsector->ceilingpic == frontsector->ceilingpic
        && backsector->floorpic == frontsector->floorpic
        && backsector->lightlevel == frontsector->lightlevel
        && curline->sidedef->midtexture == 0)
    {
        return;
    }

clippass:
    clipPassWallSegment(x1, x2 - 1);
    return;

clipsolid:
    clipSolidWallSegment(x1, x2 - 1);
}

//
// checkBBox
// Checks BSP node/subtree bounding box.
// Returns true
//  if some part of the bbox might be visible.
//
doom_boolean checkBBox(fixed_t* bspcoord)
{
    int boxx;
    int boxy;
    int boxpos;

    fixed_t x1;
    fixed_t y1;
    fixed_t x2;
    fixed_t y2;

    angle_t angle1;
    angle_t angle2;
    angle_t span;
    angle_t tspan;

    cliprange_t* start;

    int sx1;
    int sx2;

    // Find the corners of the box
    // that define the edges from current viewpoint.
    if (viewx <= bspcoord[BOXLEFT])
        boxx = 0;
    else if (viewx < bspcoord[BOXRIGHT])
        boxx = 1;
    else
        boxx = 2;

    if (viewy >= bspcoord[BOXTOP])
        boxy = 0;
    else if (viewy > bspcoord[BOXBOTTOM])
        boxy = 1;
    else
        boxy = 2;

    boxpos = (boxy << 2) + boxx;
    if (boxpos == 5)
        return true;

    x1 = bspcoord[checkcoord[boxpos][0]];
    y1 = bspcoord[checkcoord[boxpos][1]];
    x2 = bspcoord[checkcoord[boxpos][2]];
    y2 = bspcoord[checkcoord[boxpos][3]];

    // check clip list for an open space
    angle1 = R_PointToAngle(x1, y1) - viewangle;
    angle2 = R_PointToAngle(x2, y2) - viewangle;

    span = angle1 - angle2;

    // Sitting on a line?
    if (span >= ANG180)
        return true;

    tspan = angle1 + clipangle;

    if (tspan > 2 * clipangle)
    {
        tspan -= 2 * clipangle;

        // Totally off the left edge?
        if (tspan >= span)
            return false;

        angle1 = clipangle;
    }
    tspan = clipangle - angle2;
    if (tspan > 2 * clipangle)
    {
        tspan -= 2 * clipangle;

        // Totally off the left edge?
        if (tspan >= span)
            return false;

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4146)
#endif
        angle2 = -clipangle;
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    }

    // Find the first clippost
    //  that touches the source post
    //  (adjacent pixels are touching).
    angle1 = (angle1 + ANG90) >> ANGLETOFINESHIFT;
    angle2 = (angle2 + ANG90) >> ANGLETOFINESHIFT;
    sx1 = viewangletox[angle1];
    sx2 = viewangletox[angle2];

    // Does not cross a pixel.
    if (sx1 == sx2)
        return false;
    sx2--;

    start = solidsegs;
    while (start->last < sx2)
        start++;

    if (sx1 >= start->first && sx2 <= start->last)
    {
        // The clippost contains the new span.
        return false;
    }

    return true;
}

//
// subsector
// Determine floor/ceiling planes.
// Add sprites of things in sector.
// Draw one or more line segments.
//
void subsector(int num)
{
    int count;
    seg_t* line;
    subsector_t* sub;

#ifdef RANGECHECK
    if (num >= numsubsectors)
    {
        //I_Error("Error: subsector: ss %i with numss = %i",
        //        num,
        //        numsubsectors);

        doom_strcpy(error_buf, "Error: subsector: ss ");
        doom_concat(error_buf, doom_itoa(num, 10));
        doom_concat(error_buf, " with numss = ");
        doom_concat(error_buf, doom_itoa(numsubsectors, 10));
        I_Error(error_buf);
    }
#endif

    sscount++;
    sub = &subsectors[num];
    frontsector = sub->sector;
    count = sub->numlines;
    line = &segs[sub->firstline];

    if (frontsector->floorheight < viewz)
    {
        floorplane = Doom::findPlane(frontsector->floorheight,
                                 frontsector->floorpic,
                                 frontsector->lightlevel);
    }
    else
        floorplane = nullptr;

    if (frontsector->ceilingheight > viewz || frontsector->ceilingpic == skyflatnum)
    {
        ceilingplane = Doom::findPlane(frontsector->ceilingheight,
                                   frontsector->ceilingpic,
                                   frontsector->lightlevel);
    }
    else
        ceilingplane = nullptr;

    Doom::addSprites(frontsector);

    while (count--)
    {
        addLine(line);
        line++;
    }
}

//
// RenderBSPNode
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.
void renderBSPNode(int bspnum)
{
    node_t* bsp;
    int side;

    // Found a subsector?
    if (bspnum & NF_SUBSECTOR)
    {
        if (bspnum == -1)
            subsector(0);
        else
            subsector(bspnum & (~NF_SUBSECTOR));
        return;
    }

    bsp = &nodes[bspnum];

    // Decide which side the view point is on.
    side = R_PointOnSide(viewx, viewy, bsp);

    // Recursively divide front space.
    renderBSPNode(bsp->children[side]);

    // Possibly divide back space.
    if (checkBBox(bsp->bbox[side ^ 1]))
        renderBSPNode(bsp->children[side ^ 1]);
}
} // namespace Doom
