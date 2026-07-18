#include "Sight.h"

#include "../doom_config.h" // error_buf, doom_itoa (RANGECHECK)
#include "../doomdef.h"
#include "../p_local.h"
#include "../r_state.h" // sectors, segs, nodes, rejectmatrix, validcount

#include "Clip.h"
#include "SightScratch.h"
#include "ValidCount.h"

#include "../Host/System.h"
namespace Doom
{
namespace
{
// The sight line and its endpoint; p_sight's own scratch (unlike topslope/
// bottomslope, which it shares with the aim and so live in Clip). Now on the Engine
// (Sim/SightScratch.h, moved by the file-scope-statics sweep - REFACTOR.md, Step 5); the vanilla
// names are references onto that member (sightcounts as a reference-to-array).
fixed_t& sightzstart = sightScratch().sightzstart; // eye z of looker
DivLine& strace = sightScratch().strace; // from t1 to t2
fixed_t& t2x = sightScratch().t2x;
fixed_t& t2y = sightScratch().t2y;

int (&sightcounts)[2] = sightScratch().sightcounts;

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
    fixed_t dx;
    fixed_t dy;
    fixed_t left;
    fixed_t right;

    if (!node.delta.x.raw)
    {
        if (x == node.origin.x.raw)
            return 2;

        if (x <= node.origin.x.raw)
            return node.delta.y.raw > 0;

        return node.delta.y.raw < 0;
    }

    if (!node.delta.y.raw)
    {
        if (x == node.origin.y.raw)
            return 2;

        if (y <= node.origin.y.raw)
            return node.delta.x.raw < 0;

        return node.delta.x.raw > 0;
    }

    dx = (x - node.origin.x.raw);
    dy = (y - node.origin.y.raw);

    left = (node.delta.y.raw >> FRACBITS) * (dx >> FRACBITS);
    right = (dy >> FRACBITS) * (node.delta.x.raw >> FRACBITS);

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

    den = FixedMul(v1.delta.y.raw >> 8, v2.delta.x.raw)
          - FixedMul(v1.delta.x.raw >> 8, v2.delta.y.raw);

    if (den == 0)
        return 0;

    num = FixedMul((v1.origin.x.raw - v2.origin.x.raw) >> 8, v1.delta.y.raw)
          + FixedMul((v2.origin.y.raw - v1.origin.y.raw) >> 8, v1.delta.x.raw);
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
        s1 = divlineSide(v1->x, v1->y, strace);
        s2 = divlineSide(v2->x, v2->y, strace);

        // line isn't crossed?
        if (s1 == s2)
            continue;

        divl.origin = {Fixed {v1->x}, Fixed {v1->y}};
        divl.delta = {Fixed {v2->x - v1->x}, Fixed {v2->y - v1->y}};
        s1 = divlineSide(strace.origin.x.raw, strace.origin.y.raw, divl);
        s2 = divlineSide(t2x, t2y, divl);

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

        frac = interceptVector2(strace, divl);

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
    Node* bsp;
    int side;

    if (bspnum & NF_SUBSECTOR)
    {
        if (bspnum == -1)
            return crossSubsector(0);
        else
            return crossSubsector(bspnum & (~NF_SUBSECTOR));
    }

    bsp = &nodes[bspnum];

    // The node's partition line, which vanilla read by casting the node itself to
    // a divline_t - its first four fields are the same four numbers. Named now
    // that DivLine is a real type, at no cost: the same four loads either way.
    const DivLine partition {{Fixed {bsp->x}, Fixed {bsp->y}},
                             {Fixed {bsp->dx}, Fixed {bsp->dy}}};

    // decide which side the start point is on
    side = divlineSide(strace.origin.x.raw, strace.origin.y.raw, partition);
    if (side == 2)
        side = 0; // an "on" should cross both sides

    // cross the starting side
    if (!crossBSPNode(bsp->children[side]))
        return false;

    // the partition plane is crossed here
    if (side == divlineSide(t2x, t2y, partition))
    {
        // the line doesn't touch the other side
        return true;
    }

    // cross the ending side
    return crossBSPNode(bsp->children[side ^ 1]);
}
} // namespace

bool checkSight(Mobj* t1, Mobj* t2)
{
    Clip& clip = Doom::clip();

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
        sightcounts[0]++;

        // can't possibly be connected
        return false;
    }

    // An unobstructed LOS is possible. Now look from eyes of t1 to any part of t2.
    sightcounts[1]++;

    validCount().validcount++;

    sightzstart = t1->z + t1->height - (t1->height >> 2);
    clip.topslope = (t2->z + t2->height) - sightzstart;
    clip.bottomslope = (t2->z) - sightzstart;

    strace.origin = {Fixed {t1->x}, Fixed {t1->y}};
    t2x = t2->x;
    t2y = t2->y;
    strace.delta = {Fixed {t2->x - t1->x}, Fixed {t2->y - t1->y}};

    // the head node is the last node output
    return crossBSPNode(numnodes - 1);
}
} // namespace Doom
