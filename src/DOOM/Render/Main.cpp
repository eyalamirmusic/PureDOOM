// Rewritten out of vanilla r_main into namespace Doom.
//
// The renderer's per-frame setup and the geometry it rests on: point-to-angle and
// point-on-side (with the load-bearing "one below north" quirk preserved), the light
// and texture-mapping tables, view-size handling, R_SetupFrame/R_RenderPlayerView.
// r_main.cpp shims the R_ names and owns the view-state globals (viewx/viewangle/...,
// validcount, the drawer pointers other renderer files switch, the pending-view
// flags d_main/g_game/DOOM read); r_main's own bookkeeping (framecount, setdetail,
// transcolfunc) is file-local here. renderInit is Doom::renderInit (renamed to avoid a clash
// with Setup's init).

#include "../Host/Platform.h" // doom_abs, doom_print

#include "../Game/GameDefs.h"

#include "../Game/NetTypes.h"
#include "../Game/MapSpawns.h"

#include "Main.h"
#include "RenderMainState.h"

#include "../Game/Net.h"
#include "../Math/Trig.h"
#include "../Sim/ValidCount.h"
#include "BSP.h"
#include "Data.h"
#include "Draw.h"
#include "Lighting.h"
#include "PlaneScratch.h"
#include "Planes.h"
#include "RenderScratch.h"
#include "SegState.h"
#include "Sky.h"
#include "SpriteState.h"
#include "Things.h"
#include "ViewPoint.h"
#include "ViewProjection.h"
#include "ViewWindow.h"
#include "../Math/BBox.h"
#include "../UI/MenuSettings.h"
#include "../Render/GraphicsData.h"
#include "../Sim/Level.h"
#define FIELDOFVIEW 2048 // Fineangles in the SCREENWIDTH wide window.

// menuSettings().detailLevel/menuSettings().screenblocks are config-backed Engine members (UI/MenuSettings.h); references.

namespace Doom
{
// framecount/setdetail now live on the Engine (Render/RenderMainState.h, moved by the
// file-scope-statics sweep - REFACTOR.md, Step 5); the vanilla names are references onto that member.
static int& framecount = renderMainState().framecount;
static int& setdetail = renderMainState().setdetail;
void (*transcolfunc)();

// Forward declarations so call order needs no rearranging.
void addPointToBox(int x, int y, fixed_t* box);
int pointOnSide(fixed_t x, fixed_t y, Node* node);
int pointOnSegSide(fixed_t x, fixed_t y, Seg* line);
angle_t pointToAngle(fixed_t x, fixed_t y);
angle_t pointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);
fixed_t pointToDist(fixed_t x, fixed_t y);
void initPointToAngle();
fixed_t scaleFromGlobalAngle(angle_t visangle);
void initTables();
void initTextureMapping();
void initLightTables();
void setViewSize(int blocks, int detail);
void executeSetViewSize();
void renderInit();
SubSector* pointInSubsector(fixed_t x, fixed_t y);
void setupFrame(Player& player);
void renderPlayerView(Player& player);

// Vanilla takes x/y as plain ints although they are raw fixed-point coordinates
// (the box is fixed_t). Nothing calls this, so the signature is left alone and the
// raw values are wrapped here.
void addPointToBox(int x, int y, fixed_t* box)
{
    if (fixed_t {x} < box[BOXLEFT])
        box[BOXLEFT] = fixed_t {x};
    if (fixed_t {x} > box[BOXRIGHT])
        box[BOXRIGHT] = fixed_t {x};
    if (fixed_t {y} < box[BOXBOTTOM])
        box[BOXBOTTOM] = fixed_t {y};
    if (fixed_t {y} > box[BOXTOP])
        box[BOXTOP] = fixed_t {y};
}

//
// pointOnSide
// Traverse BSP (sub) tree,
//  check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
int pointOnSide(fixed_t x, fixed_t y, Node* node)
{
    fixed_t dx;
    fixed_t dy;
    fixed_t left;
    fixed_t right;

    if (!node->dx)
    {
        if (x <= node->x)
            return node->dy.isPositive();

        return node->dy.isNegative();
    }
    if (!node->dy)
    {
        if (y <= node->y)
            return node->dx.isNegative();

        return node->dx.isPositive();
    }

    dx = (x - node->x);
    dy = (y - node->y);

    // Try to quickly decide by looking at sign bits.
    if ((node->dy.raw ^ node->dx.raw ^ dx.raw ^ dy.raw) & 0x80000000)
    {
        if ((node->dy.raw ^ dx.raw) & 0x80000000)
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

int pointOnSegSide(fixed_t x, fixed_t y, Seg* line)
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
            return ldy.isPositive();

        return ldy.isNegative();
    }
    if (!ldy)
    {
        if (y <= ly)
            return ldx.isNegative();

        return ldx.isPositive();
    }

    dx = (x - lx);
    dy = (y - ly);

    // Try to quickly decide by looking at sign bits.
    if ((ldy.raw ^ ldx.raw ^ dx.raw ^ dy.raw) & 0x80000000)
    {
        if ((ldy.raw ^ dx.raw) & 0x80000000)
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
    auto& pt = viewPoint();

    x -= pt.viewx;
    y -= pt.viewy;

    if ((!x) && (!y))
        return angle_t {};

    if (!x.isNegative())
    {
        // x >=0
        if (!y.isNegative())
        {
            // y>= 0

            if (x > y)
            {
                // octant 0
                return tantoangle[Doom::slopeDiv(y.raw, x.raw)];
            }
            else
            {
                // octant 1
                return ANG90 - angle_t {1} - tantoangle[Doom::slopeDiv(x.raw, y.raw)];
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
                return -tantoangle[Doom::slopeDiv(y.raw, x.raw)];
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
            }
            else
            {
                // octant 7
                return ANG270 + tantoangle[Doom::slopeDiv(x.raw, y.raw)];
            }
        }
    }
    else
    {
        // x<0
        x = -x;

        if (!y.isNegative())
        {
            // y>= 0
            if (x > y)
            {
                // octant 3
                return ANG180 - angle_t {1} - tantoangle[Doom::slopeDiv(y.raw, x.raw)];
            }
            else
            {
                // octant 2
                return ANG90 + tantoangle[Doom::slopeDiv(x.raw, y.raw)];
            }
        }
        else
        {
            // y<0
            y = -y;

            if (x > y)
            {
                // octant 4
                return ANG180 + tantoangle[Doom::slopeDiv(y.raw, x.raw)];
            }
            else
            {
                // octant 5
                return ANG270 - angle_t {1} - tantoangle[Doom::slopeDiv(x.raw, y.raw)];
            }
        }
    }
    return angle_t {};
}

angle_t pointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
    auto& pt = viewPoint();

    pt.viewx = x1;
    pt.viewy = y1;

    return pointToAngle(x2, y2);
}

fixed_t pointToDist(fixed_t x, fixed_t y)
{
    int angle;
    fixed_t dx;
    fixed_t dy;
    fixed_t temp;
    fixed_t dist;

    auto& pt = viewPoint();

    dx = doom_abs(x - pt.viewx);
    dy = doom_abs(y - pt.viewy);

    if (dy > dx)
    {
        temp = dx;
        dx = dy;
        dy = temp;
    }

    const auto fine = (tantoangle[FixedDiv(dy, dx).raw >> DBITS] + ANG90).fineIndex();

    // use as cosine
    dist = FixedDiv(dx, finesine[fine]);

    return dist;
}

//
// initPointToAngle
//
void initPointToAngle() {}

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
    angle_t anglea {};
    angle_t angleb {};
    fixed_t sinea;
    fixed_t sineb;
    fixed_t num;
    fixed_t den;

    auto& scratch = renderScratch();

    anglea = ANG90 + (visangle - viewPoint().viewangle);
    angleb = ANG90 + (visangle - scratch.rw_normalangle);

    // both sines are allways positive
    sinea = finesine[anglea.fineIndex()];
    sineb = finesine[angleb.fineIndex()];
    num = FixedMul(viewProjection().projection, sineb) << viewWindow().detailshift;
    den = FixedMul(scratch.rw_distance, sinea);

    if (den > num >> 16)
    {
        scale = FixedDiv(num, den);

        if (scale > 64 * FRACUNIT)
            scale = 64 * FRACUNIT;
        else if (scale < fixed_t {256})
            scale = fixed_t {256};
    }
    else
        scale = 64 * FRACUNIT;

    return scale;
}

//
// initTables
//
void initTables() {}

//
// initTextureMapping
//
void initTextureMapping()
{
    int i;
    int t;
    fixed_t focallength;

    auto& proj = viewProjection();
    auto& view = viewWindow();

    // Use tangent table to generate viewangletox:
    // viewangletox will give the next greatest x
    // after the view angle.
    //
    // Calc focallength
    // so FIELDOFVIEW angles covers SCREENWIDTH.
    focallength =
        FixedDiv(proj.centerxfrac, finetangent[FINEANGLES / 4 + FIELDOFVIEW / 2]);

    for (i = 0; i < FINEANGLES / 2; i++)
    {
        if (finetangent[i] > FRACUNIT * 2)
            t = -1;
        else if (finetangent[i] < -FRACUNIT * 2)
            t = view.viewwidth + 1;
        else
        {
            // t is dual-purpose: a raw fixed product here, a screen column below.
            t = FixedMul(finetangent[i], focallength).raw;
            t = (proj.centerxfrac.raw - t + fracUnit - 1) >> FRACBITS;

            if (t < -1)
                t = -1;
            else if (t > view.viewwidth + 1)
                t = view.viewwidth + 1;
        }
        proj.viewangletox[i] = t;
    }

    // Scan viewangletox[] to generate xtoviewangle[]:
    // xtoviewangle will give the smallest view angle
    // that maps to x.
    for (int x = 0; x <= view.viewwidth; x++)
    {
        i = 0;
        while (proj.viewangletox[i] > x)
            i++;
        proj.xtoviewangle[x] = angle_t {(unsigned) i << ANGLETOFINESHIFT} - ANG90;
    }

    // Take out the fencepost cases from viewangletox.
    for (i = 0; i < FINEANGLES / 2; i++)
    {
        t = FixedMul(finetangent[i], focallength).raw;
        t = proj.centerx - t;

        if (proj.viewangletox[i] == -1)
            proj.viewangletox[i] = 0;
        else if (proj.viewangletox[i] == view.viewwidth + 1)
            proj.viewangletox[i] = view.viewwidth;
    }

    proj.clipangle = proj.xtoviewangle[0];
}

//
// initLightTables
// Only inits the zlight table,
// because the scalelight table changes with view size.
//

void initLightTables()
{
    int level;
    int startmap;
    int scale;

    auto& lights = lighting();

    // Calculate the light levels to use
    //  for each level / distance combination.
    for (int i = 0; i < LIGHTLEVELS; i++)
    {
        startmap = ((LIGHTLEVELS - 1 - i) * 2) * NUMCOLORMAPS / LIGHTLEVELS;
        for (int j = 0; j < MAXLIGHTZ; j++)
        {
            // scale is dual-purpose: a raw fixed quotient, then a light index.
            scale = FixedDiv(SCREENWIDTH / 2 * FRACUNIT,
                             fixed_t {(j + 1) << LIGHTZSHIFT})
                        .raw;
            scale >>= LIGHTSCALESHIFT;
            level = startmap - scale / DISTMAP;

            if (level < 0)
                level = 0;

            if (level >= NUMCOLORMAPS)
                level = NUMCOLORMAPS - 1;

            lights.zlight[i][j] = colormaps + level * 256;
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
    auto& view = viewWindow();

    view.setsizeneeded = true;
    view.setblocks = blocks;
    setdetail = detail;
}

//
// executeSetViewSize
//
void executeSetViewSize()
{
    fixed_t cosadj;
    fixed_t dy;
    int i;
    int j;
    int level;
    int startmap;

    auto& view = viewWindow();
    auto& proj = viewProjection();
    auto& sprites = spriteState();
    auto& plane = planeScratch();
    auto& lights = lighting();

    view.setsizeneeded = false;

    if (view.setblocks == 11)
    {
        view.scaledviewwidth = SCREENWIDTH;
        view.viewheight = SCREENHEIGHT;
    }
    else
    {
        view.scaledviewwidth = view.setblocks * 32;
        view.viewheight = (view.setblocks * 168 / 10) & ~7;
    }

    view.detailshift = setdetail;
    view.viewwidth = view.scaledviewwidth >> view.detailshift;

    proj.centery = view.viewheight / 2;
    proj.centerx = view.viewwidth / 2;
    proj.centerxfrac = Doom::Fixed::fromInt(proj.centerx);
    proj.centeryfrac = Doom::Fixed::fromInt(proj.centery);
    proj.projection = proj.centerxfrac;

    if (!view.detailshift)
    {
        colfunc = basecolfunc = Doom::drawColumn;
        fuzzcolfunc = Doom::drawFuzzColumn;
        transcolfunc = Doom::drawTranslatedColumn;
        spanfunc = Doom::drawSpan;
    }
    else
    {
        colfunc = basecolfunc = Doom::drawColumnLow;
        fuzzcolfunc = Doom::drawFuzzColumn;
        transcolfunc = Doom::drawTranslatedColumn;
        spanfunc = Doom::drawSpanLow;
    }

    Doom::initBuffer(view.scaledviewwidth, view.viewheight);

    initTextureMapping();

    // psprite scales
    sprites.pspritescale = FRACUNIT * view.viewwidth / SCREENWIDTH;
    sprites.pspriteiscale = FRACUNIT * SCREENWIDTH / view.viewwidth;

    // thing clipping
    for (i = 0; i < view.viewwidth; i++)
        sprites.screenheightarray[i] = view.viewheight;

    // planes
    for (i = 0; i < view.viewheight; i++)
    {
        dy = Doom::Fixed::fromInt(i - view.viewheight / 2) + FRACUNIT / 2;
        dy = doom_abs(dy);
        plane.yslope[i] =
            FixedDiv((view.viewwidth << view.detailshift) / 2 * FRACUNIT, dy);
    }

    for (i = 0; i < view.viewwidth; i++)
    {
        cosadj = doom_abs(finecosine[proj.xtoviewangle[i].fineIndex()]);
        plane.distscale[i] = FixedDiv(FRACUNIT, cosadj);
    }

    // Calculate the light levels to use
    //  for each level / scale combination.
    for (i = 0; i < LIGHTLEVELS; i++)
    {
        startmap = ((LIGHTLEVELS - 1 - i) * 2) * NUMCOLORMAPS / LIGHTLEVELS;
        for (j = 0; j < MAXLIGHTSCALE; j++)
        {
            level =
                startmap
                - j * SCREENWIDTH / (view.viewwidth << view.detailshift) / DISTMAP;

            if (level < 0)
                level = 0;

            if (level >= NUMCOLORMAPS)
                level = NUMCOLORMAPS - 1;

            lights.scalelight[i][j] = colormaps + level * 256;
        }
    }
}

//
// renderInit
//
void renderInit()
{
    Doom::initData();
    doom_print("\nR_InitData");
    initPointToAngle();
    doom_print("\nR_InitPointToAngle");
    initTables();
    // viewwidth / viewheight / menuSettings().detailLevel are set by the defaults
    doom_print("\nR_InitTables");

    setViewSize(menuSettings().screenblocks, menuSettings().detailLevel);
    Doom::initPlanes();
    doom_print("\nR_InitPlanes");
    initLightTables();
    doom_print("\nR_InitLightTables");
    Doom::initSkyMap();
    doom_print("\nR_InitSkyMap");
    Doom::initTranslationTables();
    doom_print("\nR_InitTranslationsTables");

    framecount = 0;
}

//
// pointInSubsector
//
SubSector* pointInSubsector(fixed_t x, fixed_t y)
{
    Node* node;
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
void setupFrame(Player& player)
{
    auto& pt = viewPoint();
    auto& lights = lighting();

    pt.viewplayer = &player;
    pt.viewx = player.mo->x;
    pt.viewy = player.mo->y;
    pt.viewangle = player.mo->angle;
    lights.extralight = player.extralight;

    pt.viewz = player.viewz;

    pt.viewsin = finesine[pt.viewangle.fineIndex()];
    pt.viewcos = finecosine[pt.viewangle.fineIndex()];

    renderScratch().sscount = 0;

    if (player.fixedcolormap)
    {
        lights.fixedcolormap =
            colormaps + player.fixedcolormap * 256 * sizeof(LightTable);

        segState().walllights = lights.scalelightfixed;

        for (int i = 0; i < MAXLIGHTSCALE; i++)
            lights.scalelightfixed[i] = lights.fixedcolormap;
    }
    else
        lights.fixedcolormap = nullptr;

    framecount++;
    validCount().validcount++;
}

//
// R_RenderView
//
void renderPlayerView(Player& player)
{
    setupFrame(player);

    // Clear buffers.
    Doom::clearClipSegs();
    Doom::clearDrawSegs();
    Doom::clearPlanes();
    Doom::clearSprites();

    // check for new console commands.
    Doom::netUpdate();

    // The head node is the last node output.
    Doom::renderBSPNode(numnodes - 1);

    // Check for new console commands.
    Doom::netUpdate();

    Doom::drawPlanes();

    // Check for new console commands.
    Doom::netUpdate();

    Doom::drawMasked();

    // Check for new console commands.
    Doom::netUpdate();
}
} // namespace Doom

// ---------------------------------------------------------------------------
// Global-scope data that was r_main.cpp. It stays at :: scope because these are the
// vanilla names other translation units (and the eacp port) still link against.
// ---------------------------------------------------------------------------
// increment every time a check is made - a Doom::ValidCount owned by the Engine now
// (the one scalar owned by no subsystem); this vanilla name is a reference onto it.

// The light selection is a Doom::Lighting owned by the Engine now; these vanilla names
// are references onto it. fixedcolormap/extralight are set per frame by R_SetupFrame,
// the scalelight/zlight tables built once by R_InitLightTables.

// The screen projection is a Doom::ViewProjection owned by the Engine now; these
// vanilla names are references onto it. R_ExecuteSetViewSize (Render/Main.cpp) writes
// through them when the view size changes.

// The subsector counter is a Doom::RenderScratch member (an Engine member) now; a
// reference onto it.

// The view point (camera) is a Doom::ViewPoint owned by the Engine now; these
// vanilla names are references onto it for the renderer code still reading them as
// globals. R_SetupFrame (Render/Main.cpp) writes through them each frame.

// 0 = high, 1 = low. Part of the view-sizing state in Doom::ViewWindow now; a reference
// onto it.

//
// precalculated math tables (references onto the Engine's ViewProjection). The two
// tables are references-to-array, so their type is unchanged and every indexed read
// resolves exactly as before.
//

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X.

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.

// References-to-array onto Doom::Lighting, so the type and every indexed read (the
// walllights = scalelight[light] row assignment included) are unchanged.

// bumped light from gun blasts

// The pending view-size request, stashed by R_SetViewSize (Render/Main.cpp) for
// R_ExecuteSetViewSize. Part of Doom::ViewWindow now; references onto it.

void (*colfunc)();
void (*basecolfunc)();
void (*fuzzcolfunc)();
void (*spanfunc)();

//
// R_AddPointToBox
// Expand a given bbox
// so that it encloses a given point.
//
