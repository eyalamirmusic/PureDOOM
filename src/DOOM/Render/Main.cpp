// Rewritten out of vanilla r_main into namespace Doom.
//
// The renderer's per-frame setup and the geometry it rests on: point-to-angle and
// point-on-side (with the load-bearing "one below north" quirk preserved), the light
// and texture-mapping tables, view-size handling, R_SetupFrame/R_RenderPlayerView.
// r_main.cpp shims the R_ names and owns the view-state globals (viewx/viewangle/...,
// validcount, the pending-view flags d_main/g_game/DOOM read); r_main's own
// bookkeeping (setdetail) is file-local here. renderInit is renderInit (renamed to
// avoid a clash with Setup's init).
//
// executeSetViewSize chooses the column and span drawers for the detail mode; they
// live on the Drawers cluster (Render/Drawers.h) rather than in the four raw
// function-pointer globals this file used to define.

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

// menuSettings().detailLevel/menuSettings().screenblocks are config-backed Engine members (UI/MenuSettings.h); references.

namespace Doom
{

// Fineangles in the SCREENWIDTH wide window.
constexpr int FIELDOFVIEW = 2048;

// setdetail now lives on the Engine (Render/RenderMainState.h, moved by the
// file-scope-statics sweep - REFACTOR.md, Step 5); it was a reference onto that member until the
// file-local-alias sweep (REFACTOR.md, Step 9 strand (a)) retired it - setViewSize and executeSetViewSize
// each reach it through renderMainState() directly.
// Forward declarations so call order needs no rearranging.
void addPointToBox(int x, int y, Fixed* box);
int pointOnSide(Fixed x, Fixed y, Node& node);
int pointOnSegSide(Fixed x, Fixed y, Seg& line);
Angle pointToAngle(Fixed x, Fixed y);
Angle pointToAngle2(Fixed x1, Fixed y1, Fixed x2, Fixed y2);
Fixed pointToDist(Fixed x, Fixed y);
void initPointToAngle();
Fixed scaleFromGlobalAngle(Angle visangle);
void initTables();
void initTextureMapping();
void initLightTables();
void setViewSize(int blocks, int detail);
void executeSetViewSize();
void renderInit();
SubSector* pointInSubsector(Fixed x, Fixed y);
void setupFrame(Player& player);
void renderPlayerView(Player& player);

// Vanilla takes x/y as plain ints although they are raw fixed-point coordinates
// (the box is Fixed). Nothing calls this, so the signature is left alone and the
// raw values are wrapped here.
void addPointToBox(int x, int y, Fixed* box)
{
    if (Fixed {x} < box[boxLeft])
        box[boxLeft] = Fixed {x};
    if (Fixed {x} > box[boxRight])
        box[boxRight] = Fixed {x};
    if (Fixed {y} < box[boxBottom])
        box[boxBottom] = Fixed {y};
    if (Fixed {y} > box[boxTop])
        box[boxTop] = Fixed {y};
}

//
// pointOnSide
// Traverse BSP (sub) tree,
//  check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
int pointOnSide(Fixed x, Fixed y, Node& node)
{
    Fixed dx;
    Fixed dy;
    Fixed left;
    Fixed right;

    if (!node.dx)
    {
        if (x <= node.x)
            return node.dy.isPositive();

        return node.dy.isNegative();
    }
    if (!node.dy)
    {
        if (y <= node.y)
            return node.dx.isNegative();

        return node.dx.isPositive();
    }

    dx = (x - node.x);
    dy = (y - node.y);

    // Try to quickly decide by looking at sign bits.
    if ((node.dy.raw ^ node.dx.raw ^ dx.raw ^ dy.raw) & 0x80000000)
    {
        if ((node.dy.raw ^ dx.raw) & 0x80000000)
        {
            // (left is negative)
            return 1;
        }
        return 0;
    }

    left = FixedMul(node.dy >> fracBits, dx);
    right = FixedMul(dy, node.dx >> fracBits);

    if (right < left)
    {
        // front side
        return 0;
    }
    // back side
    return 1;
}

int pointOnSegSide(Fixed x, Fixed y, Seg& line)
{
    Fixed lx;
    Fixed ly;
    Fixed ldx;
    Fixed ldy;
    Fixed dx;
    Fixed dy;
    Fixed left;
    Fixed right;

    lx = line.v1->x;
    ly = line.v1->y;

    ldx = line.v2->x - lx;
    ldy = line.v2->y - ly;

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

    left = FixedMul(ldy >> fracBits, dx);
    right = FixedMul(dy, ldx >> fracBits);

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

Angle pointToAngle(Fixed x, Fixed y)
{
    auto& pt = viewPoint();

    x -= pt.viewx;
    y -= pt.viewy;

    if ((!x) && (!y))
        return Angle {};

    if (!x.isNegative())
    {
        // x >=0
        if (!y.isNegative())
        {
            // y>= 0

            if (x > y)
            {
                // octant 0
                return tantoangle()[slopeDiv(y.raw, x.raw)];
            }
            else
            {
                // octant 1
                return ang90 - Angle {1} - tantoangle()[slopeDiv(x.raw, y.raw)];
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
                return -tantoangle()[slopeDiv(y.raw, x.raw)];
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
            }
            else
            {
                // octant 7
                return ang270 + tantoangle()[slopeDiv(x.raw, y.raw)];
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
                return ang180 - Angle {1} - tantoangle()[slopeDiv(y.raw, x.raw)];
            }
            else
            {
                // octant 2
                return ang90 + tantoangle()[slopeDiv(x.raw, y.raw)];
            }
        }
        else
        {
            // y<0
            y = -y;

            if (x > y)
            {
                // octant 4
                return ang180 + tantoangle()[slopeDiv(y.raw, x.raw)];
            }
            else
            {
                // octant 5
                return ang270 - Angle {1} - tantoangle()[slopeDiv(x.raw, y.raw)];
            }
        }
    }
    return Angle {};
}

Angle pointToAngle2(Fixed x1, Fixed y1, Fixed x2, Fixed y2)
{
    auto& pt = viewPoint();

    pt.viewx = x1;
    pt.viewy = y1;

    return pointToAngle(x2, y2);
}

Fixed pointToDist(Fixed x, Fixed y)
{
    Fixed dx;
    Fixed dy;
    Fixed temp;
    Fixed dist;

    auto& pt = viewPoint();

    dx = doom_abs(x - pt.viewx);
    dy = doom_abs(y - pt.viewy);

    if (dy > dx)
    {
        temp = dx;
        dx = dy;
        dy = temp;
    }

    const auto fine =
        (tantoangle()[FixedDiv(dy, dx).raw >> slopeToFixedShift] + ang90)
            .fineIndex();

    // use as cosine
    dist = FixedDiv(dx, finesine()[fine]);

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
Fixed scaleFromGlobalAngle(Angle visangle)
{
    Fixed scale;
    Angle anglea {};
    Angle angleb {};
    Fixed sinea;
    Fixed sineb;
    Fixed num;
    Fixed den;

    auto& scratch = renderScratch();

    anglea = ang90 + (visangle - viewPoint().viewangle);
    angleb = ang90 + (visangle - scratch.rw_normalangle);

    // both sines are allways positive
    sinea = finesine()[anglea.fineIndex()];
    sineb = finesine()[angleb.fineIndex()];
    num = FixedMul(viewProjection().projection, sineb) << viewWindow().detailshift;
    den = FixedMul(scratch.rw_distance, sinea);

    if (den > num >> 16)
    {
        scale = FixedDiv(num, den);

        if (scale > 64 * FRACUNIT)
            scale = 64 * FRACUNIT;
        else if (scale < Fixed {256})
            scale = Fixed {256};
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
    Fixed focallength;

    auto& proj = viewProjection();
    auto& view = viewWindow();

    // Use tangent table to generate viewangletox:
    // viewangletox will give the next greatest x
    // after the view angle.
    //
    // Calc focallength
    // so FIELDOFVIEW angles covers SCREENWIDTH.
    focallength =
        FixedDiv(proj.centerxfrac, finetangent()[fineAngles / 4 + FIELDOFVIEW / 2]);

    for (i = 0; i < fineAngles / 2; i++)
    {
        if (finetangent()[i] > FRACUNIT * 2)
            t = -1;
        else if (finetangent()[i] < -FRACUNIT * 2)
            t = view.viewwidth + 1;
        else
        {
            // t is dual-purpose: a raw fixed product here, a screen column below.
            t = FixedMul(finetangent()[i], focallength).raw;
            t = (proj.centerxfrac.raw - t + fracUnit - 1) >> fracBits;

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
        proj.xtoviewangle[x] =
            Angle {(unsigned) i << Angle::angleToFineShift} - ang90;
    }

    // Take out the fencepost cases from viewangletox.
    for (i = 0; i < fineAngles / 2; i++)
    {
        t = FixedMul(finetangent()[i], focallength).raw;
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
            scale =
                FixedDiv(SCREENWIDTH / 2 * FRACUNIT, Fixed {(j + 1) << LIGHTZSHIFT})
                    .raw;
            scale >>= LIGHTSCALESHIFT;
            level = startmap - scale / DISTMAP;

            if (level < 0)
                level = 0;

            if (level >= NUMCOLORMAPS)
                level = NUMCOLORMAPS - 1;

            lights.zlight[i][j] = graphicsData().colormaps + level * 256;
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
    renderMainState().setdetail = detail;
}

//
// executeSetViewSize
//
void executeSetViewSize()
{
    Fixed cosadj;
    Fixed dy;
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

    view.detailshift = renderMainState().setdetail;
    view.viewwidth = view.scaledviewwidth >> view.detailshift;

    proj.centery = view.viewheight / 2;
    proj.centerx = view.viewwidth / 2;
    proj.centerxfrac = Fixed::fromInt(proj.centerx);
    proj.centeryfrac = Fixed::fromInt(proj.centery);
    proj.projection = proj.centerxfrac;

    auto& drawer = drawers();

    if (!view.detailshift)
    {
        drawer.column = drawer.baseColumn = drawColumn;
        drawer.span = drawSpan;
    }
    else
    {
        drawer.column = drawer.baseColumn = drawColumnLow;
        drawer.span = drawSpanLow;
    }

    drawer.fuzzColumn = drawFuzzColumn;
    drawer.translatedColumn = drawTranslatedColumn;

    initBuffer(view.scaledviewwidth, view.viewheight);

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
        dy = Fixed::fromInt(i - view.viewheight / 2) + FRACUNIT / 2;
        dy = doom_abs(dy);
        plane.yslope[i] =
            FixedDiv((view.viewwidth << view.detailshift) / 2 * FRACUNIT, dy);
    }

    for (i = 0; i < view.viewwidth; i++)
    {
        cosadj = doom_abs(finecosine()[proj.xtoviewangle[i].fineIndex()]);
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

            lights.scalelight[i][j] = graphicsData().colormaps + level * 256;
        }
    }
}

//
// renderInit
//
void renderInit()
{
    initData();
    host().print("\nR_InitData");
    initPointToAngle();
    host().print("\nR_InitPointToAngle");
    initTables();
    // viewwidth / viewheight / menuSettings().detailLevel are set by the defaults
    host().print("\nR_InitTables");

    setViewSize(menuSettings().screenblocks, menuSettings().detailLevel);
    initPlanes();
    host().print("\nR_InitPlanes");
    initLightTables();
    host().print("\nR_InitLightTables");
    initSkyMap();
    host().print("\nR_InitSkyMap");
    initTranslationTables();
    host().print("\nR_InitTranslationsTables");
}

//
// pointInSubsector
//
SubSector* pointInSubsector(Fixed x, Fixed y)
{
    Node* node;
    int side;
    int nodenum;

    // single subsector is a special case
    if (!level().nodes.size())
        return level().subsectors.data();

    nodenum = level().nodes.size() - 1;

    while (!(nodenum & NF_SUBSECTOR))
    {
        node = &level().nodes[nodenum];
        side = pointOnSide(x, y, *node);
        nodenum = node->children[side];
    }

    return &level().subsectors[nodenum & ~NF_SUBSECTOR];
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

    pt.viewsin = finesine()[pt.viewangle.fineIndex()];
    pt.viewcos = finecosine()[pt.viewangle.fineIndex()];

    if (player.fixedcolormap)
    {
        lights.fixedcolormap = graphicsData().colormaps
                               + player.fixedcolormap * 256 * sizeof(LightTable);

        segState().walllights = lights.scalelightfixed.data();

        lights.scalelightfixed.fill(lights.fixedcolormap);
    }
    else
        lights.fixedcolormap = nullptr;

    validCount().validcount++;
}

//
// R_RenderView
//
void renderPlayerView(Player& player)
{
    setupFrame(player);

    // Clear buffers.
    clearClipSegs();
    clearDrawSegs();
    clearPlanes();
    clearSprites();

    // check for new console commands.
    netUpdate();

    // The head node is the last node output.
    renderBSPNode(level().nodes.size() - 1);

    // Check for new console commands.
    netUpdate();

    drawPlanes();

    // Check for new console commands.
    netUpdate();

    drawMasked();

    // Check for new console commands.
    netUpdate();
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

// The viewangletox[viewangle + fineAngles/4] lookup
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

//
// R_AddPointToBox
// Expand a given bbox
// so that it encloses a given point.
//
