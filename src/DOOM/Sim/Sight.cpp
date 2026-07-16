#include "Sight.h"

#include "../doom_config.h" // error_buf, doom_itoa (RANGECHECK)
#include "../doomdef.h"
#include "../i_system.h"
#include "../p_local.h"
#include "../r_state.h" // sectors, segs, nodes, rejectmatrix, validcount

#include "Clip.h"
#include "SightScratch.h"

namespace Doom
{
namespace
{
// The sight line and its endpoint; p_sight's own scratch (unlike topslope/
// bottomslope, which it shares with the aim and so live in Clip). Now on the Engine
// (Sim/SightScratch.h, moved by the file-scope-statics sweep - REFACTOR.md, Step 5); the vanilla
// names are references onto that member (sightcounts as a reference-to-array).
fixed_t& sightzstart = sightScratch().sightzstart; // eye z of looker
divline_t& strace = sightScratch().strace; // from t1 to t2
fixed_t& t2x = sightScratch().t2x;
fixed_t& t2y = sightScratch().t2y;

int (&sightcounts)[2] = sightScratch().sightcounts;

//
// P_DivlineSide
// Returns side 0 (front), 1 (back), or 2 (on).
//
int divlineSide(fixed_t x, fixed_t y, divline_t* node)
{
    fixed_t dx;
    fixed_t dy;
    fixed_t left;
    fixed_t right;

    if (!node->dx)
    {
        if (x == node->x)
            return 2;

        if (x <= node->x)
            return node->dy > 0;

        return node->dy < 0;
    }

    if (!node->dy)
    {
        if (x == node->y)
            return 2;

        if (y <= node->y)
            return node->dx < 0;

        return node->dx > 0;
    }

    dx = (x - node->x);
    dy = (y - node->y);

    left = (node->dy >> FRACBITS) * (dx >> FRACBITS);
    right = (dy >> FRACBITS) * (node->dx >> FRACBITS);

    if (right < left)
        return 0; // front side

    if (left == right)
        return 2;
    return 1; // back side
}

//
// P_InterceptVector2
// Returns the fractional intercept point along the first divline. This is only
// called by the addthings and addlines traversers.
//
fixed_t interceptVector2(divline_t* v2, divline_t* v1)
{
    fixed_t frac;
    fixed_t num;
    fixed_t den;

    den = FixedMul(v1->dy >> 8, v2->dx) - FixedMul(v1->dx >> 8, v2->dy);

    if (den == 0)
        return 0;

    num = FixedMul((v1->x - v2->x) >> 8, v1->dy)
          + FixedMul((v2->y - v1->y) >> 8, v1->dx);
    frac = FixedDiv(num, den);

    return frac;
}

//
// P_CrossSubsector
// Returns true if strace crosses the given subsector successfully.
//
doom_boolean crossSubsector(int num)
{
    Clip& clip = Doom::clip();

    seg_t* seg;
    line_t* line;
    int s1;
    int s2;
    int count;
    subsector_t* sub;
    sector_t* front;
    sector_t* back;
    fixed_t opentop;
    fixed_t openbottom;
    divline_t divl;
    vertex_t* v1;
    vertex_t* v2;
    fixed_t frac;
    fixed_t slope;

#ifdef RANGECHECK
    if (num >= numsubsectors)
    {
        doom_strcpy(error_buf, "Error: P_CrossSubsector: ss ");
        doom_concat(error_buf, doom_itoa(num, 10));
        doom_concat(error_buf, " with numss = ");
        doom_concat(error_buf, doom_itoa(numsubsectors, 10));
        I_Error(error_buf);
    }
#endif

    sub = &subsectors[num];

    // check lines
    count = sub->numlines;
    seg = &segs[sub->firstline];

    for (; count; seg++, count--)
    {
        line = seg->linedef;

        // allready checked other side?
        if (line->validcount == validcount)
            continue;

        line->validcount = validcount;

        v1 = line->v1;
        v2 = line->v2;
        s1 = divlineSide(v1->x, v1->y, &strace);
        s2 = divlineSide(v2->x, v2->y, &strace);

        // line isn't crossed?
        if (s1 == s2)
            continue;

        divl.x = v1->x;
        divl.y = v1->y;
        divl.dx = v2->x - v1->x;
        divl.dy = v2->y - v1->y;
        s1 = divlineSide(strace.x, strace.y, &divl);
        s2 = divlineSide(t2x, t2y, &divl);

        // line isn't crossed?
        if (s1 == s2)
            continue;

        // stop because it is not two sided anyway
        // might do this after updating validcount?
        if (!(line->flags & ML_TWOSIDED))
            return false;

        // crosses a two sided line
        front = seg->frontsector;
        back = seg->backsector;

        // no wall to block sight with?
        if (front->floorheight == back->floorheight
            && front->ceilingheight == back->ceilingheight)
            continue;

        // possible occluder
        // because of ceiling height differences
        if (front->ceilingheight < back->ceilingheight)
            opentop = front->ceilingheight;
        else
            opentop = back->ceilingheight;

        // because of ceiling height differences
        if (front->floorheight > back->floorheight)
            openbottom = front->floorheight;
        else
            openbottom = back->floorheight;

        // quick test for totally closed doors
        if (openbottom >= opentop)
            return false; // stop

        frac = interceptVector2(&strace, &divl);

        if (front->floorheight != back->floorheight)
        {
            slope = FixedDiv(openbottom - sightzstart, frac);
            if (slope > clip.bottomslope)
                clip.bottomslope = slope;
        }

        if (front->ceilingheight != back->ceilingheight)
        {
            slope = FixedDiv(opentop - sightzstart, frac);
            if (slope < clip.topslope)
                clip.topslope = slope;
        }

        if (clip.topslope <= clip.bottomslope)
            return false; // stop
    }

    // passed the subsector ok
    return true;
}

//
// P_CrossBSPNode
// Returns true if strace crosses the given node successfully.
//
doom_boolean crossBSPNode(int bspnum)
{
    node_t* bsp;
    int side;

    if (bspnum & NF_SUBSECTOR)
    {
        if (bspnum == -1)
            return crossSubsector(0);
        else
            return crossSubsector(bspnum & (~NF_SUBSECTOR));
    }

    bsp = &nodes[bspnum];

    // decide which side the start point is on
    side = divlineSide(strace.x, strace.y, (divline_t*) bsp);
    if (side == 2)
        side = 0; // an "on" should cross both sides

    // cross the starting side
    if (!crossBSPNode(bsp->children[side]))
        return false;

    // the partition plane is crossed here
    if (side == divlineSide(t2x, t2y, (divline_t*) bsp))
    {
        // the line doesn't touch the other side
        return true;
    }

    // cross the ending side
    return crossBSPNode(bsp->children[side ^ 1]);
}
} // namespace

bool checkSight(mobj_t* t1, mobj_t* t2)
{
    Clip& clip = Doom::clip();

    int s1;
    int s2;
    int pnum;
    int bytenum;
    int bitnum;

    // First check for trivial rejection.

    // Determine subsector entries in REJECT table.
    s1 = (int) (t1->subsector->sector - sectors);
    s2 = (int) (t2->subsector->sector - sectors);
    pnum = s1 * numsectors + s2;
    bytenum = pnum >> 3;
    bitnum = 1 << (pnum & 7);

    // Check in REJECT table.
    if (rejectmatrix[bytenum] & bitnum)
    {
        sightcounts[0]++;

        // can't possibly be connected
        return false;
    }

    // An unobstructed LOS is possible. Now look from eyes of t1 to any part of t2.
    sightcounts[1]++;

    validcount++;

    sightzstart = t1->z + t1->height - (t1->height >> 2);
    clip.topslope = (t2->z + t2->height) - sightzstart;
    clip.bottomslope = (t2->z) - sightzstart;

    strace.x = t1->x;
    strace.y = t1->y;
    t2x = t2->x;
    t2y = t2->y;
    strace.dx = t2->x - t1->x;
    strace.dy = t2->y - t1->y;

    // the head node is the last node output
    return crossBSPNode(numnodes - 1);
}
} // namespace Doom
