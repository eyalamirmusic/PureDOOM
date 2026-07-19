#include "Sight.h"
#include "Level.h"

#include "../Host/Platform.h" // error_buf, doom_itoa (RANGECHECK)
#include "../Game/GameDefs.h"
#include "MapGeometry.h" // DivLine
#include "SimDefs.h"

#include "ValidCount.h"

#include "../Host/System.h"
namespace Doom
{
namespace
{
// The sight line and its endpoint are per-call locals now (SightTrace below),
// threaded by reference through crossBSPNode/crossSubsector the same way
// topslope/bottomslope already were - they never escape checkSight's own call
// chain, so there is nothing for a file-scope alias to buy. That emptied the old
// SightScratch cluster, and its last member (the write-only sightcounts) went in
// the leftovers audit, so the cluster itself is gone rather than kept empty.
//
// P_DivlineSide
// Returns side 0 (front), 1 (back), or 2 (on).
//
// A third answer is what makes this the sight check's own side test rather than
// MapGeometry's pointOnDivlineSide: "on the line" has to be distinguishable here,
// because a sight line that runs exactly along a partition must cross both sides.
// The `x == node.origin.y` in the horizontal branch is vanilla's own typo and is
// load-bearing - every recorded demo's monster wake-ups went through it.
int divlineSide(fixed_t x, fixed_t y, const DivLine& node)
{
    if (!node.delta.x)
    {
        if (x == node.origin.x)
            return 2;

        if (x <= node.origin.x)
            return node.delta.y.isPositive();

        return node.delta.y.isNegative();
    }

    if (!node.delta.y)
    {
        if (x == node.origin.y)
            return 2;

        if (y <= node.origin.y)
            return node.delta.x.isNegative();

        return node.delta.x.isPositive();
    }

    const auto dx = x - node.origin.x;
    const auto dy = y - node.origin.y;

    // These are INTEGER products of the shifted-down values, not a fixed-point
    // multiply: vanilla declares them fixed_t but never treats them as such, and
    // Fixed::operator* would shift the product back down by fracBits and give a
    // different answer. Kept as plain ints on the raw bits.
    const int left = (node.delta.y.raw >> fracBits) * (dx.raw >> fracBits);
    const int right = (dy.raw >> fracBits) * (node.delta.x.raw >> fracBits);

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
fixed_t interceptVector2(const DivLine& v2, const DivLine& v1)
{
    fixed_t frac;
    fixed_t num;
    fixed_t den;

    den = FixedMul(v1.delta.y >> 8, v2.delta.x)
          - FixedMul(v1.delta.x >> 8, v2.delta.y);

    if (den.isZero())
        return fixed_t {};

    num = FixedMul((v1.origin.x - v2.origin.x) >> 8, v1.delta.y)
          + FixedMul((v2.origin.y - v1.origin.y) >> 8, v1.delta.x);
    frac = FixedDiv(num, den);

    return frac;
}

// The sight line and its endpoint - set once in checkSight and read-only from here
// down, so it travels as one const reference rather than four.
struct SightTrace
{
    fixed_t sightzstart; // eye z of looker
    DivLine strace; // from t1 to t2
    fixed_t t2x;
    fixed_t t2y;
};

//
// P_CrossSubsector
// Returns true if strace crosses the given subsector successfully. topslope/
// bottomslope are this sight check's own narrowing window - local to checkSight,
// not Clip - threaded down by reference the same way the aim trace threads its
// own through aimTraverse (Sim/MapAction.cpp).
//
bool crossSubsector(int num,
                    const SightTrace& trace,
                    fixed_t& topslope,
                    fixed_t& bottomslope)
{
    Seg* seg;
    Line* line;
    int s1;
    int s2;
    int count;
    SubSector* sub;
    Sector* front;
    Sector* back;
    fixed_t opentop;
    fixed_t openbottom;
    DivLine divl;
    Vertex* v1;
    Vertex* v2;
    fixed_t frac;
    fixed_t slope;

#ifdef RANGECHECK
    if (num >= numsubsectors)
    {
        doom_strcpy(error_buf, "Error: P_CrossSubsector: ss ");
        doom_concat(error_buf, doom_itoa(num, 10));
        doom_concat(error_buf, " with numss = ");
        doom_concat(error_buf, doom_itoa(numsubsectors, 10));
        fatalError(error_buf);
    }
#endif

    auto& vc = validCount();

    sub = &subsectors[num];

    // check lines
    count = sub->numlines;
    seg = &segs[sub->firstline];

    for (; count; seg++, count--)
    {
        line = seg->linedef;

        // allready checked other side?
        if (line->validcount == vc.validcount)
            continue;

        line->validcount = vc.validcount;

        v1 = line->v1;
        v2 = line->v2;
        s1 = divlineSide(v1->x, v1->y, trace.strace);
        s2 = divlineSide(v2->x, v2->y, trace.strace);

        // line isn't crossed?
        if (s1 == s2)
            continue;

        divl.origin = {Fixed {v1->x}, Fixed {v1->y}};
        divl.delta = {Fixed {v2->x - v1->x}, Fixed {v2->y - v1->y}};
        s1 = divlineSide(trace.strace.origin.x, trace.strace.origin.y, divl);
        s2 = divlineSide(trace.t2x, trace.t2y, divl);

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

        frac = interceptVector2(trace.strace, divl);

        if (front->floorheight != back->floorheight)
        {
            slope = FixedDiv(openbottom - trace.sightzstart, frac);
            if (slope > bottomslope)
                bottomslope = slope;
        }

        if (front->ceilingheight != back->ceilingheight)
        {
            slope = FixedDiv(opentop - trace.sightzstart, frac);
            if (slope < topslope)
                topslope = slope;
        }

        if (topslope <= bottomslope)
            return false; // stop
    }

    // passed the subsector ok
    return true;
}

//
// P_CrossBSPNode
// Returns true if strace crosses the given node successfully.
//
bool crossBSPNode(int bspnum,
                  const SightTrace& trace,
                  fixed_t& topslope,
                  fixed_t& bottomslope)
{
    Node* bsp;
    int side;

    if (bspnum & NF_SUBSECTOR)
    {
        if (bspnum == -1)
            return crossSubsector(0, trace, topslope, bottomslope);
        else
            return crossSubsector(
                bspnum & (~NF_SUBSECTOR), trace, topslope, bottomslope);
    }

    bsp = &nodes[bspnum];

    // The node's partition line, which vanilla read by casting the node itself to
    // a divline_t - its first four fields are the same four numbers. Named now
    // that DivLine is a real type, at no cost: the same four loads either way.
    const DivLine partition {{Fixed {bsp->x}, Fixed {bsp->y}},
                             {Fixed {bsp->dx}, Fixed {bsp->dy}}};

    // decide which side the start point is on
    side = divlineSide(trace.strace.origin.x, trace.strace.origin.y, partition);
    if (side == 2)
        side = 0; // an "on" should cross both sides

    // cross the starting side
    if (!crossBSPNode(bsp->children[side], trace, topslope, bottomslope))
        return false;

    // the partition plane is crossed here
    if (side == divlineSide(trace.t2x, trace.t2y, partition))
    {
        // the line doesn't touch the other side
        return true;
    }

    // cross the ending side
    return crossBSPNode(bsp->children[side ^ 1], trace, topslope, bottomslope);
}
} // namespace

bool checkSight(Mobj* t1, Mobj* t2)
{
    int s1;
    int s2;
    int pnum;
    int bytenum;
    int bitnum;

    // First check for trivial rejection.

    // Determine subsector entries in REJECT table.
    s1 = static_cast<int>(t1->subsector->sector - sectors);
    s2 = static_cast<int>(t2->subsector->sector - sectors);
    pnum = s1 * numsectors + s2;
    bytenum = pnum >> 3;
    bitnum = 1 << (pnum & 7);

    // Check in REJECT table.
    if (rejectmatrix[bytenum] & bitnum)
    {
        // can't possibly be connected
        return false;
    }

    // An unobstructed LOS is possible. Now look from eyes of t1 to any part of t2.
    validCount().validcount++;

    SightTrace trace;
    trace.sightzstart = t1->z + t1->height - (t1->height >> 2);
    fixed_t topslope = (t2->z + t2->height) - trace.sightzstart;
    fixed_t bottomslope = (t2->z) - trace.sightzstart;

    trace.strace.origin = {Fixed {t1->x}, Fixed {t1->y}};
    trace.t2x = t2->x;
    trace.t2y = t2->y;
    trace.strace.delta = {Fixed {t2->x - t1->x}, Fixed {t2->y - t1->y}};

    // the head node is the last node output
    return crossBSPNode(numnodes - 1, trace, topslope, bottomslope);
}
} // namespace Doom
