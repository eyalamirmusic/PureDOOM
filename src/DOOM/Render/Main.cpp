// Rewritten out of vanilla r_main into namespace Doom.
//
// The renderer's per-frame setup and the geometry it rests on: point-to-angle and
// point-on-side (with the load-bearing "one below north" quirk preserved), the light
// and texture-mapping tables, view-size handling, R_SetupFrame/R_RenderPlayerView.
// r_main.cpp shims the R_ names and owns the view-state globals (viewx/viewangle/...,
// validcount, the drawer pointers other renderer files switch, the pending-view
// flags d_main/g_game/DOOM read); r_main's own bookkeeping (framecount, setdetail,
// transcolfunc) is file-local here. renderInit is R_Init (renamed to avoid a clash
// with Setup's init).

#include "../doom_config.h" // doom_abs, doom_print

#include "../doomdef.h"

#include "../d_net.h"
#include "../doomstat.h"
#include "../m_bbox.h"
#include "../r_draw.h"
#include "../r_local.h"
#include "../r_sky.h"

#include "Main.h"
#include "RenderMainState.h"

#define FIELDOFVIEW 2048 // Fineangles in the SCREENWIDTH wide window.

extern lighttable_t**& walllights; // Doom::SegState member (Engine); reference
// detailLevel/screenblocks are config-backed Engine members (UI/MenuSettings.h); references.
extern int& detailLevel;
extern int& screenblocks;

// setsizeneeded/setblocks live in the r_main.cpp shim (d_main, g_game and DOOM.cpp
// switch them through a local extern); declared here so the setup code can too.
extern int& setsizeneeded;
extern int& setblocks;

namespace Doom
{
// framecount/setdetail now live on the Engine (Render/RenderMainState.h, moved by the
// file-scope-statics sweep - REFACTOR.md, Step 5); the vanilla names are references onto that member.
static int& framecount = renderMainState().framecount;
static int& setdetail = renderMainState().setdetail;
void (*transcolfunc)(void);

// Forward declarations so call order needs no rearranging.
void addPointToBox(int x, int y, fixed_t* box);
int pointOnSide(fixed_t x, fixed_t y, node_t* node);
int pointOnSegSide(fixed_t x, fixed_t y, seg_t* line);
angle_t pointToAngle(fixed_t x, fixed_t y);
angle_t pointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);
fixed_t pointToDist(fixed_t x, fixed_t y);
void initPointToAngle(void);
fixed_t scaleFromGlobalAngle(angle_t visangle);
void initTables(void);
void initTextureMapping(void);
void initLightTables(void);
void setViewSize(int blocks, int detail);
void executeSetViewSize(void);
void renderInit(void);
subsector_t* pointInSubsector(fixed_t x, fixed_t y);
void setupFrame(player_t* player);
void renderPlayerView(player_t* player);

void addPointToBox(int x, int y, fixed_t* box)
{
    if (x < box[BOXLEFT])
        box[BOXLEFT] = x;
    if (x > box[BOXRIGHT])
        box[BOXRIGHT] = x;
    if (y < box[BOXBOTTOM])
        box[BOXBOTTOM] = y;
    if (y > box[BOXTOP])
        box[BOXTOP] = y;
}

//
// pointOnSide
// Traverse BSP (sub) tree,
//  check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
int pointOnSide(fixed_t x, fixed_t y, node_t* node)
{
    fixed_t dx;
    fixed_t dy;
    fixed_t left;
    fixed_t right;

    if (!node->dx)
    {
        if (x <= node->x)
            return node->dy > 0;

        return node->dy < 0;
    }
    if (!node->dy)
    {
        if (y <= node->y)
            return node->dx < 0;

        return node->dx > 0;
    }

    dx = (x - node->x);
    dy = (y - node->y);

    // Try to quickly decide by looking at sign bits.
    if ((node->dy ^ node->dx ^ dx ^ dy) & 0x80000000)
    {
        if ((node->dy ^ dx) & 0x80000000)
        {
            // (left is negative)
            return 1;
        }
        return 0;
    }

    left = FixedMul(node->dy >> FRACBITS, dx);
    right = FixedMul(dy, node->dx >> FRACBITS);

    if (right < left)
    {
        // front side
        return 0;
    }
    // back side
    return 1;
}

int pointOnSegSide(fixed_t x, fixed_t y, seg_t* line)
{
    fixed_t lx;
    fixed_t ly;
    fixed_t ldx;
    fixed_t ldy;
    fixed_t dx;
    fixed_t dy;
    fixed_t left;
    fixed_t right;

    lx = line->v1->x;
    ly = line->v1->y;

    ldx = line->v2->x - lx;
    ldy = line->v2->y - ly;

    if (!ldx)
    {
        if (x <= lx)
            return ldy > 0;

        return ldy < 0;
    }
    if (!ldy)
    {
        if (y <= ly)
            return ldx < 0;

        return ldx > 0;
    }

    dx = (x - lx);
    dy = (y - ly);

    // Try to quickly decide by looking at sign bits.
    if ((ldy ^ ldx ^ dx ^ dy) & 0x80000000)
    {
        if ((ldy ^ dx) & 0x80000000)
        {
            // (left is negative)
            return 1;
        }
        return 0;
    }

    left = FixedMul(ldy >> FRACBITS, dx);
    right = FixedMul(dy, ldx >> FRACBITS);

    if (right < left)
    {
        // front side
        return 0;
    }
    // back side
    return 1;
}

//
// pointToAngle
// To get a global angle from cartesian coordinates,
//  the coordinates are flipped until they are in
//  the first octant of the coordinate system, then
//  the y (<=x) is scaled and divided by x to get a
//  tangent (slope) value which is looked up in the
//  tantoangle[] table.

angle_t pointToAngle(fixed_t x, fixed_t y)
{
    x -= viewx;
    y -= viewy;

    if ((!x) && (!y))
        return 0;

    if (x >= 0)
    {
        // x >=0
        if (y >= 0)
        {
            // y>= 0

            if (x > y)
            {
                // octant 0
                return tantoangle[SlopeDiv(y, x)];
            }
            else
            {
                // octant 1
                return ANG90 - 1 - tantoangle[SlopeDiv(x, y)];
            }
        }
        else
        {
            // y<0
            y = -y;

            if (x > y)
            {
                // octant 8
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4146)
#endif
                return -tantoangle[SlopeDiv(y, x)];
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
            }
            else
            {
                // octant 7
                return ANG270 + tantoangle[SlopeDiv(x, y)];
            }
        }
    }
    else
    {
        // x<0
        x = -x;

        if (y >= 0)
        {
            // y>= 0
            if (x > y)
            {
                // octant 3
                return ANG180 - 1 - tantoangle[SlopeDiv(y, x)];
            }
            else
            {
                // octant 2
                return ANG90 + tantoangle[SlopeDiv(x, y)];
            }
        }
        else
        {
            // y<0
            y = -y;

            if (x > y)
            {
                // octant 4
                return ANG180 + tantoangle[SlopeDiv(y, x)];
            }
            else
            {
                // octant 5
                return ANG270 - 1 - tantoangle[SlopeDiv(x, y)];
            }
        }
    }
    return 0;
}

angle_t pointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
    viewx = x1;
    viewy = y1;

    return pointToAngle(x2, y2);
}

fixed_t pointToDist(fixed_t x, fixed_t y)
{
    int angle;
    fixed_t dx;
    fixed_t dy;
    fixed_t temp;
    fixed_t dist;

    dx = doom_abs(x - viewx);
    dy = doom_abs(y - viewy);

    if (dy > dx)
    {
        temp = dx;
        dx = dy;
        dy = temp;
    }

    angle = (tantoangle[FixedDiv(dy, dx) >> DBITS] + ANG90) >> ANGLETOFINESHIFT;

    // use as cosine
    dist = FixedDiv(dx, finesine[angle]);

    return dist;
}

//
// initPointToAngle
//
void initPointToAngle(void) {}

//
// scaleFromGlobalAngle
// Returns the texture mapping scale
//  for the current line (horizontal span)
//  at the given angle.
// rw_distance must be calculated first.
//
fixed_t scaleFromGlobalAngle(angle_t visangle)
{
    fixed_t scale;
    int anglea;
    int angleb;
    int sinea;
    int sineb;
    fixed_t num;
    int den;

    anglea = ANG90 + (visangle - viewangle);
    angleb = ANG90 + (visangle - rw_normalangle);

    // both sines are allways positive
    sinea = finesine[anglea >> ANGLETOFINESHIFT];
    sineb = finesine[angleb >> ANGLETOFINESHIFT];
    num = FixedMul(projection, sineb) << detailshift;
    den = FixedMul(rw_distance, sinea);

    if (den > num >> 16)
    {
        scale = FixedDiv(num, den);

        if (scale > 64 * FRACUNIT)
            scale = 64 * FRACUNIT;
        else if (scale < 256)
            scale = 256;
    }
    else
        scale = 64 * FRACUNIT;

    return scale;
}

//
// initTables
//
void initTables(void) {}

//
// initTextureMapping
//
void initTextureMapping(void)
{
    int i;
    int x;
    int t;
    fixed_t focallength;

    // Use tangent table to generate viewangletox:
    // viewangletox will give the next greatest x
    // after the view angle.
    //
    // Calc focallength
    // so FIELDOFVIEW angles covers SCREENWIDTH.
    focallength =
        FixedDiv(centerxfrac, finetangent[FINEANGLES / 4 + FIELDOFVIEW / 2]);

    for (i = 0; i < FINEANGLES / 2; i++)
    {
        if (finetangent[i] > FRACUNIT * 2)
            t = -1;
        else if (finetangent[i] < -FRACUNIT * 2)
            t = viewwidth + 1;
        else
        {
            t = FixedMul(finetangent[i], focallength);
            t = (centerxfrac - t + FRACUNIT - 1) >> FRACBITS;

            if (t < -1)
                t = -1;
            else if (t > viewwidth + 1)
                t = viewwidth + 1;
        }
        viewangletox[i] = t;
    }

    // Scan viewangletox[] to generate xtoviewangle[]:
    // xtoviewangle will give the smallest view angle
    // that maps to x.
    for (x = 0; x <= viewwidth; x++)
    {
        i = 0;
        while (viewangletox[i] > x)
            i++;
        xtoviewangle[x] = (i << ANGLETOFINESHIFT) - ANG90;
    }

    // Take out the fencepost cases from viewangletox.
    for (i = 0; i < FINEANGLES / 2; i++)
    {
        t = FixedMul(finetangent[i], focallength);
        t = centerx - t;

        if (viewangletox[i] == -1)
            viewangletox[i] = 0;
        else if (viewangletox[i] == viewwidth + 1)
            viewangletox[i] = viewwidth;
    }

    clipangle = xtoviewangle[0];
}

//
// initLightTables
// Only inits the zlight table,
// because the scalelight table changes with view size.
//

void initLightTables(void)
{
    int i;
    int j;
    int level;
    int startmap;
    int scale;

    // Calculate the light levels to use
    //  for each level / distance combination.
    for (i = 0; i < LIGHTLEVELS; i++)
    {
        startmap = ((LIGHTLEVELS - 1 - i) * 2) * NUMCOLORMAPS / LIGHTLEVELS;
        for (j = 0; j < MAXLIGHTZ; j++)
        {
            scale = FixedDiv((SCREENWIDTH / 2 * FRACUNIT), (j + 1) << LIGHTZSHIFT);
            scale >>= LIGHTSCALESHIFT;
            level = startmap - scale / DISTMAP;

            if (level < 0)
                level = 0;

            if (level >= NUMCOLORMAPS)
                level = NUMCOLORMAPS - 1;

            zlight[i][j] = colormaps + level * 256;
        }
    }
}

//
// setViewSize
// Do not really change anything here,
//  because it might be in the middle of a refresh.
// The change will take effect next refresh.
//

void setViewSize(int blocks, int detail)
{
    setsizeneeded = true;
    setblocks = blocks;
    setdetail = detail;
}

//
// executeSetViewSize
//
void executeSetViewSize(void)
{
    fixed_t cosadj;
    fixed_t dy;
    int i;
    int j;
    int level;
    int startmap;

    setsizeneeded = false;

    if (setblocks == 11)
    {
        scaledviewwidth = SCREENWIDTH;
        viewheight = SCREENHEIGHT;
    }
    else
    {
        scaledviewwidth = setblocks * 32;
        viewheight = (setblocks * 168 / 10) & ~7;
    }

    detailshift = setdetail;
    viewwidth = scaledviewwidth >> detailshift;

    centery = viewheight / 2;
    centerx = viewwidth / 2;
    centerxfrac = centerx << FRACBITS;
    centeryfrac = centery << FRACBITS;
    projection = centerxfrac;

    if (!detailshift)
    {
        colfunc = basecolfunc = R_DrawColumn;
        fuzzcolfunc = R_DrawFuzzColumn;
        transcolfunc = R_DrawTranslatedColumn;
        spanfunc = R_DrawSpan;
    }
    else
    {
        colfunc = basecolfunc = R_DrawColumnLow;
        fuzzcolfunc = R_DrawFuzzColumn;
        transcolfunc = R_DrawTranslatedColumn;
        spanfunc = R_DrawSpanLow;
    }

    R_InitBuffer(scaledviewwidth, viewheight);

    initTextureMapping();

    // psprite scales
    pspritescale = FRACUNIT * viewwidth / SCREENWIDTH;
    pspriteiscale = FRACUNIT * SCREENWIDTH / viewwidth;

    // thing clipping
    for (i = 0; i < viewwidth; i++)
        screenheightarray[i] = viewheight;

    // planes
    for (i = 0; i < viewheight; i++)
    {
        dy = ((i - viewheight / 2) << FRACBITS) + FRACUNIT / 2;
        dy = doom_abs(dy);
        yslope[i] = FixedDiv((viewwidth << detailshift) / 2 * FRACUNIT, dy);
    }

    for (i = 0; i < viewwidth; i++)
    {
        cosadj = doom_abs(finecosine[xtoviewangle[i] >> ANGLETOFINESHIFT]);
        distscale[i] = FixedDiv(FRACUNIT, cosadj);
    }

    // Calculate the light levels to use
    //  for each level / scale combination.
    for (i = 0; i < LIGHTLEVELS; i++)
    {
        startmap = ((LIGHTLEVELS - 1 - i) * 2) * NUMCOLORMAPS / LIGHTLEVELS;
        for (j = 0; j < MAXLIGHTSCALE; j++)
        {
            level =
                startmap - j * SCREENWIDTH / (viewwidth << detailshift) / DISTMAP;

            if (level < 0)
                level = 0;

            if (level >= NUMCOLORMAPS)
                level = NUMCOLORMAPS - 1;

            scalelight[i][j] = colormaps + level * 256;
        }
    }
}

//
// renderInit
//
void renderInit(void)
{
    R_InitData();
    doom_print("\nR_InitData");
    initPointToAngle();
    doom_print("\nR_InitPointToAngle");
    initTables();
    // viewwidth / viewheight / detailLevel are set by the defaults
    doom_print("\nR_InitTables");

    setViewSize(screenblocks, detailLevel);
    R_InitPlanes();
    doom_print("\nR_InitPlanes");
    initLightTables();
    doom_print("\nR_InitLightTables");
    R_InitSkyMap();
    doom_print("\nR_InitSkyMap");
    R_InitTranslationTables();
    doom_print("\nR_InitTranslationsTables");

    framecount = 0;
}

//
// pointInSubsector
//
subsector_t* pointInSubsector(fixed_t x, fixed_t y)
{
    node_t* node;
    int side;
    int nodenum;

    // single subsector is a special case
    if (!numnodes)
        return subsectors;

    nodenum = numnodes - 1;

    while (!(nodenum & NF_SUBSECTOR))
    {
        node = &nodes[nodenum];
        side = pointOnSide(x, y, node);
        nodenum = node->children[side];
    }

    return &subsectors[nodenum & ~NF_SUBSECTOR];
}

//
// setupFrame
//
void setupFrame(player_t* player)
{
    int i;

    viewplayer = player;
    viewx = player->mo->x;
    viewy = player->mo->y;
    viewangle = player->mo->angle;
    extralight = player->extralight;

    viewz = player->viewz;

    viewsin = finesine[viewangle >> ANGLETOFINESHIFT];
    viewcos = finecosine[viewangle >> ANGLETOFINESHIFT];

    sscount = 0;

    if (player->fixedcolormap)
    {
        fixedcolormap =
            colormaps + player->fixedcolormap * 256 * sizeof(lighttable_t);

        walllights = scalelightfixed;

        for (i = 0; i < MAXLIGHTSCALE; i++)
            scalelightfixed[i] = fixedcolormap;
    }
    else
        fixedcolormap = 0;

    framecount++;
    validcount++;
}

//
// R_RenderView
//
void renderPlayerView(player_t* player)
{
    setupFrame(player);

    // Clear buffers.
    R_ClearClipSegs();
    R_ClearDrawSegs();
    R_ClearPlanes();
    R_ClearSprites();

    // check for new console commands.
    NetUpdate();

    // The head node is the last node output.
    R_RenderBSPNode(numnodes - 1);

    // Check for new console commands.
    NetUpdate();

    R_DrawPlanes();

    // Check for new console commands.
    NetUpdate();

    R_DrawMasked();

    // Check for new console commands.
    NetUpdate();
}
} // namespace Doom
