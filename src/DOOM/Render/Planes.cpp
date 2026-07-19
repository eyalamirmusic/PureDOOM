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

namespace Doom
{
// The visplane and span machinery now lives on the Engine (Render/PlaneScratch.h, moved by the
// file-scope-statics sweep - REFACTOR.md, Step 5). visplanes, openings, spanstart, cachedheight,
// cacheddistance, cachedxstep, cachedystep, lastvisplane, planezlight, planeheight, basexscale and
// baseyscale were references onto that member too until the file-local-alias sweep (REFACTOR.md,
// Step 9 strand (a)) retired them - mapPlane, clearPlanes, findPlane, checkPlane, makeSpans and
// drawPlanes each hoist planeScratch() once and reach them through it. (spanstop was dropped
// outright rather than hoisted - no reader anywhere ever set or read it. The header-externed
// lastopening stays in the r_plane shim; the dead vestigial `ceilingfunc` was deleted.)

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

    if (plane.planeheight != plane.cachedheight[y])
    {
        plane.cachedheight[y] = plane.planeheight;
        distance = plane.cacheddistance[y] =
            FixedMul(plane.planeheight, plane.yslope[y]);
        draw.ds_xstep = plane.cachedxstep[y] = FixedMul(distance, plane.basexscale);
        draw.ds_ystep = plane.cachedystep[y] = FixedMul(distance, plane.baseyscale);
    }
    else
    {
        distance = plane.cacheddistance[y];
        draw.ds_xstep = plane.cachedxstep[y];
        draw.ds_ystep = plane.cachedystep[y];
    }

    length = FixedMul(distance, plane.distscale[x1]);
    const auto angleFine =
        (pt.viewangle + viewProjection().xtoviewangle[x1]).fineIndex();
    draw.ds_xfrac = pt.viewx + FixedMul(finecosine[angleFine], length);
    draw.ds_yfrac = -pt.viewy - FixedMul(finesine[angleFine], length);

    if (lights.fixedcolormap)
        draw.ds_colormap = lights.fixedcolormap;
    else
    {
        index = distance.raw >> LIGHTZSHIFT;

        if (index >= MAXLIGHTZ)
            index = MAXLIGHTZ - 1;

        draw.ds_colormap = plane.planezlight[index];
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

    // opening / clipping determination
    for (int i = 0; i < view.viewwidth; i++)
    {
        plane.floorclip[i] = view.viewheight;
        plane.ceilingclip[i] = -1;
    }

    plane.lastvisplane = plane.visplanes.data();
    plane.lastopening = plane.openings.data();

    // texture calculation
    doom_memset(plane.cachedheight.data(), 0, sizeof(plane.cachedheight));

    // left to right mapping
    const auto angleFine = (viewPoint().viewangle - ang90).fineIndex();

    // scale will be unit scale at SCREENWIDTH/2 distance
    plane.basexscale = FixedDiv(finecosine[angleFine], proj.centerxfrac);
    plane.baseyscale = -FixedDiv(finesine[angleFine], proj.centerxfrac);
}

//
// findPlane
//
VisPlane* findPlane(fixed_t height, int picnum, int lightlevel)
{
    VisPlane* check;

    auto& plane = planeScratch();

    if (picnum == skyState().skyflatnum)
    {
        height = fixed_t {}; // all skys map together
        lightlevel = 0;
    }

    for (check = plane.visplanes.data(); check < plane.lastvisplane; check++)
    {
        if (height == check->height && picnum == check->picnum
            && lightlevel == check->lightlevel)
        {
            break;
        }
    }

    if (check < plane.lastvisplane)
        return check;

    if (plane.lastvisplane - plane.visplanes.data() == PlaneScratch::maxVisplanes)
        fatalError("Error: findPlane: no more visplanes");

    plane.lastvisplane++;

    check->height = height;
    check->picnum = picnum;
    check->lightlevel = lightlevel;
    check->minx = SCREENWIDTH;
    check->maxx = -1;

    doom_memset(check->top.data(), 0xff, sizeof(check->top));

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

    auto& plane = planeScratch();

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
    plane.lastvisplane->height = pl->height;
    plane.lastvisplane->picnum = pl->picnum;
    plane.lastvisplane->lightlevel = pl->lightlevel;

    pl = plane.lastvisplane++;
    pl->minx = start;
    pl->maxx = stop;

    doom_memset(pl->top.data(), 0xff, sizeof(pl->top));

    return pl;
}

//
// makeSpans
//
void makeSpans(int x, int t1, int b1, int t2, int b2)
{
    auto& plane = planeScratch();

    while (t1 < t2 && t1 <= b1)
    {
        mapPlane(t1, plane.spanstart[t1], x - 1);
        t1++;
    }
    while (b1 > b2 && b1 >= t1)
    {
        mapPlane(b1, plane.spanstart[b1], x - 1);
        b1--;
    }

    while (t2 < t1 && t2 <= b2)
    {
        plane.spanstart[t2] = x;
        t2++;
    }
    while (b2 > b1 && b2 >= t2)
    {
        plane.spanstart[b2] = x;
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
    auto& plane = planeScratch();

    VisPlane* pl;
    int light;
    int stop;
    int angle;

#ifdef RANGECHECK
    auto& bsp = bspScratch();

    if (bsp.ds_p - bsp.drawsegs.data() > BSPScratch::maxDrawSegs)
    {
        //fatalError("Error: drawPlanes: drawsegs overflow (%i)",
        //        ds_p - drawsegs);

        doom_strcpy(error_buf, "Error: drawPlanes: drawsegs overflow (");
        doom_concat(error_buf,
                    doom_itoa(static_cast<int>(bsp.ds_p - bsp.drawsegs.data()), 10));
        doom_concat(error_buf, ")");
        fatalError(error_buf);
    }

    if (plane.lastvisplane - plane.visplanes.data() > PlaneScratch::maxVisplanes)
    {
        //fatalError("Error: drawPlanes: visplane overflow (%i)",
        //        lastvisplane - visplanes);

        doom_strcpy(error_buf, "Error: drawPlanes: visplane overflow (");
        doom_concat(
            error_buf,
            doom_itoa(static_cast<int>(plane.lastvisplane - plane.visplanes.data()),
                      10));
        doom_concat(error_buf, ")");
        fatalError(error_buf);
    }

    if (plane.lastopening - plane.openings.data() > PlaneScratch::maxOpenings)
    {
        //fatalError("Error: drawPlanes: opening overflow (%i)",
        //        lastopening - openings);

        doom_strcpy(error_buf, "Error: drawPlanes: opening overflow (");
        doom_concat(
            error_buf,
            doom_itoa(static_cast<int>(plane.lastopening - plane.openings.data()),
                      10));
        doom_concat(error_buf, ")");
        fatalError(error_buf);
    }
#endif

    for (pl = plane.visplanes.data(); pl < plane.lastvisplane; pl++)
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
                    angle =
                        ((pt.viewangle + proj.xtoviewangle[x]) >> ANGLETOSKYSHIFT)
                            .raw;
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

        plane.planeheight = doom_abs(pl->height - pt.viewz);
        light = (pl->lightlevel >> LIGHTSEGSHIFT) + lights.extralight;

        if (light >= LIGHTLEVELS)
            light = LIGHTLEVELS - 1;

        if (light < 0)
            light = 0;

        plane.planezlight = lights.zlight[light].data();

        // Deliberately one outside the declared bounds at each end - see VisPlane
        // in RenderTypes.h, where pad1..pad4 exist to absorb exactly these
        // accesses and a static_assert pins the layout that puts them in the
        // padding. Both the sentinel writes and the loop's [x - 1] / [x] reads
        // step outside: x runs to maxx + 1, so top and bottom are each touched at
        // [minx - 1] and [maxx + 1].
        //
        // Reached through .data() rather than operator[] because EA::Array wraps
        // std::array, and MSVC's debug STL bounds-checks std::array::operator[]
        // against the *declared* extent - it cannot know about the padding, and
        // aborts on the very access this design intends. The bytes touched are
        // identical either way; only the assertion goes.
        //
        // It did not present as an assertion, either. The debug CRT reports
        // through a modal dialog, so under ctest the binary stopped dead with no
        // output and no CPU and the whole suite hung on demo1. Tests/TestMain.cpp
        // now routes those reports to stderr so the next one says what it is.
        auto* top = pl->top.data();
        auto* bottom = pl->bottom.data();

        top[pl->maxx + 1] = 0xff;
        top[pl->minx - 1] = 0xff;

        stop = pl->maxx + 1;

        for (int x = pl->minx; x <= stop; x++)
            makeSpans(x, top[x - 1], bottom[x - 1], top[x], bottom[x]);
    }
}
} // namespace Doom
