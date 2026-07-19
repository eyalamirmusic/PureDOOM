// Rewritten out of vanilla r_bsp into namespace Doom.
//
// BSP traversal: walk the tree front-to-back for the view point, clip each
// subsector's segs against the solid-wall ranges, and hand visible wall ranges to
// r_segs and floor/ceiling to r_plane. r_bsp.cpp shims the R_ names and owns the
// drawseg list and clip ranges other renderer files read. Golden-neutral.

#include "../Host/Platform.h"

#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"

#include "../Game/SkyState.h"
#include "BSP.h"
#include "BSPScratch.h"
#include "RenderScratch.h"
#include "SolidSegs.h"
#include "ViewPoint.h"
#include "ViewProjection.h"
#include "ViewWindow.h"

#include "Planes.h"
#include "Things.h"
#include <ea_data_structures/Structures/Array.h>

// Doom::storeWallRange lives in r_segs; declared so the BSP walk can hand it ranges.
#include "Segs.h"
#include "../Host/System.h"
#include "Main.h"
#include "../Math/BBox.h"
#include "../Sim/Level.h"
void Doom::storeWallRange(int start, int stop);

#define MAXSEGS 32

namespace Doom
{
// ClipRange and the solidsegs/newend clip ranges now live on the Engine (Render/SolidSegs.h, moved
// by the file-scope-statics sweep - REFACTOR.md, Step 5). The type moved to the header so solidsegs
// could become a member (an anonymous-struct typedef in the .cpp cannot be named there). solidsegs
// and newend were both references onto the member until the file-local-alias sweep (REFACTOR.md,
// Step 9 strand (a)) retired them; clipSolidWallSegment, clipPassWallSegment, clearClipSegs and
// checkBBox each hoist solidSegs() once and reach its members through it.

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
void addLine(Seg* line);
bool checkBBox(fixed_t* bspcoord);
void subsector(int num);
void renderBSPNode(int bspnum);

void clearDrawSegs()
{
    auto& bsp = bspScratch();

    bsp.ds_p = bsp.drawsegs;
}

//
// clipSolidWallSegment
// Does handle solid walls,
//  e.g. single sided LineDefs (middle texture)
//  that entirely block the view.
//
void clipSolidWallSegment(int first, int last)
{
    ClipRange* next;
    ClipRange* start;

    auto& solid = solidSegs();

    // Find the first range that touches the range
    //  (adjacent pixels are touching).
    start = solid.solidsegs;
    while (start->last < first - 1)
        start++;

    if (first < start->first)
    {
        if (last < start->first - 1)
        {
            // Post is entirely visible (above start),
            //  so insert a new clippost.
            Doom::storeWallRange(first, last);
            next = solid.newend;
            solid.newend++;

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

    while (next++ != solid.newend)
    {
        // Remove a post.
        *++start = *next;
    }

    solid.newend = start + 1;
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
    ClipRange* start;

    auto& solid = solidSegs();

    // Find the first range that touches the range
    //  (adjacent pixels are touching).
    start = solid.solidsegs;
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
    auto& solid = solidSegs();

    solid.solidsegs[0].first = -0x7fffffff;
    solid.solidsegs[0].last = -1;
    solid.solidsegs[1].first = viewWindow().viewwidth;
    solid.solidsegs[1].last = 0x7fffffff;
    solid.newend = solid.solidsegs + 2;
}

//
// addLine
// Clips the given segment
// and adds any visible pieces to the line list.
//
void addLine(Seg* line)
{
    int x1;
    int x2;
    angle_t angle1;
    angle_t angle2;
    angle_t span;
    angle_t tspan;

    auto& bsp = bspScratch();
    auto& pt = viewPoint();
    auto& proj = viewProjection();

    bsp.curline = line;

    // OPTIMIZE: quickly reject orthogonal back sides.
    angle1 = Doom::pointToAngle(line->v1->x, line->v1->y);
    angle2 = Doom::pointToAngle(line->v2->x, line->v2->y);

    // Clip to view edges.
    // OPTIMIZE: make constant out of 2*clipangle (FIELDOFVIEW).
    span = angle1 - angle2;

    // Back side? I.e. backface culling?
    if (span >= ang180)
        return;

    // Global angle needed by segcalc.
    renderScratch().rw_angle1 = angle1;
    angle1 -= pt.viewangle;
    angle2 -= pt.viewangle;

    tspan = angle1 + proj.clipangle;
    if (tspan > proj.clipangle * 2u)
    {
        tspan -= proj.clipangle * 2u;

        // Totally off the left edge?
        if (tspan >= span)
            return;

        angle1 = proj.clipangle;
    }
    tspan = proj.clipangle - angle2;
    if (tspan > proj.clipangle * 2u)
    {
        tspan -= proj.clipangle * 2u;

        // Totally off the left edge?
        if (tspan >= span)
            return;
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4146)
#endif
        angle2 = -proj.clipangle;
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    }

    // The seg is in the view range,
    // but not necessarily visible.
    const auto fine1 = (angle1 + ang90).fineIndex();
    const auto fine2 = (angle2 + ang90).fineIndex();
    x1 = proj.viewangletox[fine1];
    x2 = proj.viewangletox[fine2];

    // Does not cross a pixel?
    if (x1 == x2)
        return;

    bsp.backsector = line->backsector;

    // Single sided line?
    if (!bsp.backsector)
        goto clipsolid;

    // Closed door.
    if (bsp.backsector->ceilingheight <= bsp.frontsector->floorheight
        || bsp.backsector->floorheight >= bsp.frontsector->ceilingheight)
        goto clipsolid;

    // Window.
    if (bsp.backsector->ceilingheight != bsp.frontsector->ceilingheight
        || bsp.backsector->floorheight != bsp.frontsector->floorheight)
        goto clippass;

    // Reject empty lines used for triggers
    //  and special events.
    // Identical floor and ceiling on both sides,
    // identical light levels on both sides,
    // and no middle texture.
    if (bsp.backsector->ceilingpic == bsp.frontsector->ceilingpic
        && bsp.backsector->floorpic == bsp.frontsector->floorpic
        && bsp.backsector->lightlevel == bsp.frontsector->lightlevel
        && bsp.curline->sidedef->midtexture == 0)
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
bool checkBBox(fixed_t* bspcoord)
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

    ClipRange* start;

    int sx1;
    int sx2;

    auto& pt = viewPoint();
    auto& proj = viewProjection();
    auto& solid = solidSegs();

    // Find the corners of the box
    // that define the edges from current viewpoint.
    if (pt.viewx <= bspcoord[BOXLEFT])
        boxx = 0;
    else if (pt.viewx < bspcoord[BOXRIGHT])
        boxx = 1;
    else
        boxx = 2;

    if (pt.viewy >= bspcoord[BOXTOP])
        boxy = 0;
    else if (pt.viewy > bspcoord[BOXBOTTOM])
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
    angle1 = Doom::pointToAngle(x1, y1) - pt.viewangle;
    angle2 = Doom::pointToAngle(x2, y2) - pt.viewangle;

    span = angle1 - angle2;

    // Sitting on a line?
    if (span >= ang180)
        return true;

    tspan = angle1 + proj.clipangle;

    if (tspan > proj.clipangle * 2u)
    {
        tspan -= proj.clipangle * 2u;

        // Totally off the left edge?
        if (tspan >= span)
            return false;

        angle1 = proj.clipangle;
    }
    tspan = proj.clipangle - angle2;
    if (tspan > proj.clipangle * 2u)
    {
        tspan -= proj.clipangle * 2u;

        // Totally off the left edge?
        if (tspan >= span)
            return false;

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4146)
#endif
        angle2 = -proj.clipangle;
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    }

    // Find the first clippost
    //  that touches the source post
    //  (adjacent pixels are touching).
    const auto angle1Fine = (angle1 + ang90).fineIndex();
    const auto angle2Fine = (angle2 + ang90).fineIndex();
    sx1 = proj.viewangletox[angle1Fine];
    sx2 = proj.viewangletox[angle2Fine];

    // Does not cross a pixel.
    if (sx1 == sx2)
        return false;
    sx2--;

    start = solid.solidsegs;
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
    Seg* line;
    SubSector* sub;

#ifdef RANGECHECK
    if (num >= numsubsectors)
    {
        //fatalError("Error: subsector: ss %i with numss = %i",
        //        num,
        //        numsubsectors);

        doom_strcpy(error_buf, "Error: subsector: ss ");
        doom_concat(error_buf, doom_itoa(num, 10));
        doom_concat(error_buf, " with numss = ");
        doom_concat(error_buf, doom_itoa(numsubsectors, 10));
        fatalError(error_buf);
    }
#endif

    auto& bsp = bspScratch();
    auto& scratch = renderScratch();
    auto& pt = viewPoint();

    sub = &subsectors[num];
    bsp.frontsector = sub->sector;
    count = sub->numlines;
    line = &segs[sub->firstline];

    if (bsp.frontsector->floorheight < pt.viewz)
    {
        scratch.floorplane = Doom::findPlane(bsp.frontsector->floorheight,
                                             bsp.frontsector->floorpic,
                                             bsp.frontsector->lightlevel);
    }
    else
        scratch.floorplane = nullptr;

    if (bsp.frontsector->ceilingheight > pt.viewz
        || bsp.frontsector->ceilingpic == skyState().skyflatnum)
    {
        scratch.ceilingplane = Doom::findPlane(bsp.frontsector->ceilingheight,
                                               bsp.frontsector->ceilingpic,
                                               bsp.frontsector->lightlevel);
    }
    else
        scratch.ceilingplane = nullptr;

    Doom::addSprites(bsp.frontsector);

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
    Node* bsp;
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
    auto& pt = viewPoint();
    side = Doom::pointOnSide(pt.viewx, pt.viewy, bsp);

    // Recursively divide front space.
    renderBSPNode(bsp->children[side]);

    // Possibly divide back space.
    if (checkBBox(bsp->bbox[side ^ 1]))
        renderBSPNode(bsp->children[side ^ 1]);
}
} // namespace Doom
