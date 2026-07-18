// Rewritten out of vanilla r_plane into namespace Doom.
//
// Floor and ceiling (visplane) rendering: map a flat span, find/merge visplanes,
// and draw them, plus the sky columns. r_plane.cpp shims the R_ names and owns the
// visplane arrays and span state other renderer files index; r_plane's private
// bookkeeping is file-local. Golden-neutral (frame goldens).

#include "../Host/Platform.h"

#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"
#include "../Wad/WadFile.h"

#include "../Game/SkyState.h"

#include "BSPScratch.h"
#include "DrawState.h"
#include "GraphicsData.h"
#include "Lighting.h"
#include "PlaneScratch.h"
#include "Planes.h"
#include "SpriteState.h"
#include "ViewPoint.h"
#include "ViewProjection.h"
#include "ViewWindow.h"

#include "Data.h"
#include "../Host/System.h"
#include "../Render/Main.h"
#include "../Render/Sky.h"
#define MAXVISPLANES 128
#define MAXOPENINGS SCREENWIDTH * 64

namespace Doom
{
// The visplane and span machinery now lives on the Engine (Render/PlaneScratch.h, moved by the
// file-scope-statics sweep - REFACTOR.md, Step 5). The vanilla names are references onto that member;
// read by no other file. (The header-externed lastopening stays in the r_plane shim; the dead
// vestigial `ceilingfunc` was deleted.)
static VisPlane (&visplanes)[MAXVISPLANES] = planeScratch().visplanes;
static VisPlane*& lastvisplane = planeScratch().lastvisplane;
static short (&openings)[MAXOPENINGS] = planeScratch().openings;
static int (&spanstart)[SCREENHEIGHT] = planeScratch().spanstart;
static int (&spanstop)[SCREENHEIGHT] = planeScratch().spanstop;
static LightTable**& planezlight = planeScratch().planezlight;
static fixed_t& planeheight = planeScratch().planeheight;
static fixed_t& basexscale = planeScratch().basexscale;
static fixed_t& baseyscale = planeScratch().baseyscale;
static fixed_t (&cachedheight)[SCREENHEIGHT] = planeScratch().cachedheight;
static fixed_t (&cacheddistance)[SCREENHEIGHT] = planeScratch().cacheddistance;
static fixed_t (&cachedxstep)[SCREENHEIGHT] = planeScratch().cachedxstep;
static fixed_t (&cachedystep)[SCREENHEIGHT] = planeScratch().cachedystep;

// Forward declarations so call order needs no rearranging.
void initPlanes();
void mapPlane(int y, int x1, int x2);
void clearPlanes();
VisPlane* findPlane(fixed_t height, int picnum, int lightlevel);
VisPlane* checkPlane(VisPlane* pl, int start, int stop);
void makeSpans(int x, int t1, int b1, int t2, int b2);
void drawPlanes();

void initPlanes()
{
    // Doh!
}

//
// mapPlane
//
// Uses global vars:
//  planeheight
//  ds_source
//  basexscale
//  baseyscale
//  viewx
//  viewy
//
// BASIC PRIMITIVE
//
void mapPlane(int y, int x1, int x2)
{
    auto& draw = drawState();
    auto& plane = planeScratch();
    auto& pt = viewPoint();
    auto& lights = lighting();

    angle_t angle;
    fixed_t distance;
    fixed_t length;
    unsigned index;

#ifdef RANGECHECK
    auto& view = viewWindow();

    if (x2 < x1 || x1 < 0 || x2 >= view.viewwidth
        || static_cast<unsigned>(y) > static_cast<unsigned>(view.viewheight))
    {
        //fatalError("Error: mapPlane: %i, %i at %i", x1, x2, y);

        doom_strcpy(error_buf, "Error: mapPlane: ");
        doom_concat(error_buf, doom_itoa(x1, 10));
        doom_concat(error_buf, ", ");
        doom_concat(error_buf, doom_itoa(x2, 10));
        doom_concat(error_buf, " at ");
        doom_concat(error_buf, doom_itoa(y, 10));
        fatalError(error_buf);
    }
#endif

    if (planeheight != cachedheight[y])
    {
        cachedheight[y] = planeheight;
        distance = cacheddistance[y] = FixedMul(planeheight, plane.yslope[y]);
        draw.ds_xstep = cachedxstep[y] = FixedMul(distance, basexscale);
        draw.ds_ystep = cachedystep[y] = FixedMul(distance, baseyscale);
    }
    else
    {
        distance = cacheddistance[y];
        draw.ds_xstep = cachedxstep[y];
        draw.ds_ystep = cachedystep[y];
    }

    length = FixedMul(distance, plane.distscale[x1]);
    angle = (pt.viewangle + viewProjection().xtoviewangle[x1]) >> ANGLETOFINESHIFT;
    draw.ds_xfrac = pt.viewx + FixedMul(finecosine[angle], length);
    draw.ds_yfrac = -pt.viewy - FixedMul(finesine[angle], length);

    if (lights.fixedcolormap)
        draw.ds_colormap = lights.fixedcolormap;
    else
    {
        index = distance >> LIGHTZSHIFT;

        if (index >= MAXLIGHTZ)
            index = MAXLIGHTZ - 1;

        draw.ds_colormap = planezlight[index];
    }

    draw.ds_y = y;
    draw.ds_x1 = x1;
    draw.ds_x2 = x2;

    // high or low detail
    spanfunc();
}

//
// clearPlanes
// At begining of frame.
//
void clearPlanes()
{
    auto& plane = planeScratch();
    auto& view = viewWindow();
    auto& proj = viewProjection();

    angle_t angle;

    // opening / clipping determination
    for (int i = 0; i < view.viewwidth; i++)
    {
        plane.floorclip[i] = view.viewheight;
        plane.ceilingclip[i] = -1;
    }

    lastvisplane = visplanes;
    plane.lastopening = openings;

    // texture calculation
    doom_memset(cachedheight, 0, sizeof(cachedheight));

    // left to right mapping
    angle = (viewPoint().viewangle - ANG90) >> ANGLETOFINESHIFT;

    // scale will be unit scale at SCREENWIDTH/2 distance
    basexscale = FixedDiv(finecosine[angle], proj.centerxfrac);
    baseyscale = -FixedDiv(finesine[angle], proj.centerxfrac);
}

//
// findPlane
//
VisPlane* findPlane(fixed_t height, int picnum, int lightlevel)
{
    VisPlane* check;

    if (picnum == skyState().skyflatnum)
    {
        height = 0; // all skys map together
        lightlevel = 0;
    }

    for (check = visplanes; check < lastvisplane; check++)
    {
        if (height == check->height && picnum == check->picnum
            && lightlevel == check->lightlevel)
        {
            break;
        }
    }

    if (check < lastvisplane)
        return check;

    if (lastvisplane - visplanes == MAXVISPLANES)
        fatalError("Error: findPlane: no more visplanes");

    lastvisplane++;

    check->height = height;
    check->picnum = picnum;
    check->lightlevel = lightlevel;
    check->minx = SCREENWIDTH;
    check->maxx = -1;

    doom_memset(check->top, 0xff, sizeof(check->top));

    return check;
}

//
// checkPlane
//
VisPlane* checkPlane(VisPlane* pl, int start, int stop)
{
    int intrl;
    int intrh;
    int unionl;
    int unionh;
    int x;

    if (start < pl->minx)
    {
        intrl = pl->minx;
        unionl = start;
    }
    else
    {
        unionl = pl->minx;
        intrl = start;
    }

    if (stop > pl->maxx)
    {
        intrh = pl->maxx;
        unionh = stop;
    }
    else
    {
        unionh = pl->maxx;
        intrh = stop;
    }

    for (x = intrl; x <= intrh; x++)
        if (pl->top[x] != 0xff)
            break;

    if (x > intrh)
    {
        pl->minx = unionl;
        pl->maxx = unionh;

        // use the same one
        return pl;
    }

    // make a new visplane
    lastvisplane->height = pl->height;
    lastvisplane->picnum = pl->picnum;
    lastvisplane->lightlevel = pl->lightlevel;

    pl = lastvisplane++;
    pl->minx = start;
    pl->maxx = stop;

    doom_memset(pl->top, 0xff, sizeof(pl->top));

    return pl;
}

//
// makeSpans
//
void makeSpans(int x, int t1, int b1, int t2, int b2)
{
    while (t1 < t2 && t1 <= b1)
    {
        mapPlane(t1, spanstart[t1], x - 1);
        t1++;
    }
    while (b1 > b2 && b1 >= t1)
    {
        mapPlane(b1, spanstart[b1], x - 1);
        b1--;
    }

    while (t2 < t1 && t2 <= b2)
    {
        spanstart[t2] = x;
        t2++;
    }
    while (b2 > b1 && b2 >= t2)
    {
        spanstart[b2] = x;
        b2--;
    }
}

//
// drawPlanes
// At the end of each frame.
//
void drawPlanes()
{
    auto& draw = drawState();
    auto& sky = skyState();
    auto& pt = viewPoint();
    auto& proj = viewProjection();
    auto& lights = lighting();

    VisPlane* pl;
    int light;
    int stop;
    int angle;

#ifdef RANGECHECK
    auto& bsp = bspScratch();

    if (bsp.ds_p - bsp.drawsegs > MAXDRAWSEGS)
    {
        //fatalError("Error: drawPlanes: drawsegs overflow (%i)",
        //        ds_p - drawsegs);

        doom_strcpy(error_buf, "Error: drawPlanes: drawsegs overflow (");
        doom_concat(error_buf,
                    doom_itoa(static_cast<int>(bsp.ds_p - bsp.drawsegs), 10));
        doom_concat(error_buf, ")");
        fatalError(error_buf);
    }

    if (lastvisplane - visplanes > MAXVISPLANES)
    {
        //fatalError("Error: drawPlanes: visplane overflow (%i)",
        //        lastvisplane - visplanes);

        doom_strcpy(error_buf, "Error: drawPlanes: visplane overflow (");
        doom_concat(error_buf,
                    doom_itoa(static_cast<int>(lastvisplane - visplanes), 10));
        doom_concat(error_buf, ")");
        fatalError(error_buf);
    }

    if (planeScratch().lastopening - openings > MAXOPENINGS)
    {
        //fatalError("Error: drawPlanes: opening overflow (%i)",
        //        lastopening - openings);

        doom_strcpy(error_buf, "Error: drawPlanes: opening overflow (");
        doom_concat(
            error_buf,
            doom_itoa(static_cast<int>(planeScratch().lastopening - openings), 10));
        doom_concat(error_buf, ")");
        fatalError(error_buf);
    }
#endif

    for (pl = visplanes; pl < lastvisplane; pl++)
    {
        if (pl->minx > pl->maxx)
            continue;

        // sky flat
        if (pl->picnum == sky.skyflatnum)
        {
            draw.dc_iscale = spriteState().pspriteiscale >> viewWindow().detailshift;

            // Sky is allways drawn full bright,
            //  i.e. colormaps[0] is used.
            // Because of this hack, sky is not affected
            //  by INVUL inverse mapping.
            draw.dc_colormap = colormaps;
            draw.dc_texturemid = sky.skytexturemid;
            for (int x = pl->minx; x <= pl->maxx; x++)
            {
                draw.dc_yl = pl->top[x];
                draw.dc_yh = pl->bottom[x];

                if (draw.dc_yl <= draw.dc_yh)
                {
                    angle = (pt.viewangle + proj.xtoviewangle[x]) >> ANGLETOSKYSHIFT;
                    draw.dc_x = x;
                    draw.dc_source = Doom::getColumn(sky.skytexture, angle);
                    colfunc();
                }
            }
            continue;
        }

        // regular flat
        draw.ds_source = static_cast<byte*>(Doom::cacheLumpNum(
            graphicsData().firstflat + flattranslation[pl->picnum]));

        planeheight = doom_abs(pl->height - pt.viewz);
        light = (pl->lightlevel >> LIGHTSEGSHIFT) + lights.extralight;

        if (light >= LIGHTLEVELS)
            light = LIGHTLEVELS - 1;

        if (light < 0)
            light = 0;

        planezlight = lights.zlight[light];

        pl->top[pl->maxx + 1] = 0xff;
        pl->top[pl->minx - 1] = 0xff;

        stop = pl->maxx + 1;

        for (int x = pl->minx; x <= stop; x++)
        {
            makeSpans(
                x, pl->top[x - 1], pl->bottom[x - 1], pl->top[x], pl->bottom[x]);
        }
    }
}
} // namespace Doom
