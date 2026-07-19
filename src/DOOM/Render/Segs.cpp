// Rewritten out of vanilla r_segs into namespace Doom.
//
// Wall (seg) rendering: draw a wall range's textured columns, the masked mid-texture
// pass for two-sided lines, and store a visible wall range for later. r_segs.cpp
// shims the R_ names and owns the per-column wall state r_bsp/r_plane share; its own
// scratch is file-local. Golden-neutral.

#include "../Host/Platform.h"

#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"

#include "../Game/SkyState.h"

#include "BSPScratch.h"
#include "DrawState.h"
#include "Lighting.h"
#include "PlaneScratch.h"
#include "RenderScratch.h"
#include "SegState.h"
#include "Segs.h"
#include "SpriteState.h"
#include "ViewPoint.h"
#include "ViewProjection.h"
#include "ViewWindow.h"
#include "WallScratch.h"

#include "Data.h"
#include "Planes.h"
#include "Things.h"
#include "../Host/System.h"
#include "Main.h"
#include "../Render/GraphicsData.h"

// These are Doom::SegState members (Engine), exported by the r_segs.cpp shim; references onto
// them - and as references, not plain externs, since Render/Segs writes them (a write through a
// plain extern would clobber the reference's pointer, the skytexturemid trap).

namespace Doom
{

constexpr int HEIGHTBITS = 12;
constexpr int HEIGHTUNIT = 1 << HEIGHTBITS;

// The per-wall-segment rendering intermediates now live on the Engine (Render/WallScratch.h, moved
// by the file-scope-statics sweep - REFACTOR.md, Step 5); read by no other file. All twenty were
// references onto that member until the file-local-alias sweep (REFACTOR.md, Step 9 strand (a))
// retired them - renderMaskedSegRange, renderSegLoop and storeWallRange each hoist wallScratch() once
// (renderSegLoop is the per-column inner loop, so this matters for more than style: a per-access
// wallScratch() call there would be a real per-pixel cost no golden would catch) and reach every
// member through it.

// Forward declarations so call order needs no rearranging.
void renderMaskedSegRange(DrawSeg* ds, int x1, int x2);
void renderSegLoop();
void storeWallRange(int start, int stop);

void renderMaskedSegRange(DrawSeg* ds, int x1, int x2)
{
    auto& bsp = bspScratch();
    auto& seg = segState();
    auto& draw = drawState();
    auto& lights = lighting();
    auto& sprites = spriteState();
    auto& pt = viewPoint();
    auto& wall = wallScratch();

    unsigned index;
    Column* col;
    int lightnum;
    int texnum;

    // Calculate light table.
    // Use different light tables
    //   for horizontal / vertical / diagonal. Diagonal?
    // OPTIMIZE: get rid of LIGHTSEGSHIFT globally
    bsp.curline = ds->curline;
    bsp.frontsector = bsp.curline->frontsector;
    bsp.backsector = bsp.curline->backsector;
    texnum = texturetranslation[bsp.curline->sidedef->midtexture];

    lightnum = (bsp.frontsector->lightlevel >> LIGHTSEGSHIFT) + lights.extralight;

    if (bsp.curline->v1->y == bsp.curline->v2->y)
        lightnum--;
    else if (bsp.curline->v1->x == bsp.curline->v2->x)
        lightnum++;

    if (lightnum < 0)
        seg.walllights = lights.scalelight[0].data();
    else if (lightnum >= LIGHTLEVELS)
        seg.walllights = lights.scalelight[LIGHTLEVELS - 1].data();
    else
        seg.walllights = lights.scalelight[lightnum].data();

    seg.maskedtexturecol = ds->maskedtexturecol;

    wall.rw_scalestep = ds->scalestep;
    sprites.spryscale = ds->scale1 + (x1 - ds->x1) * wall.rw_scalestep;
    sprites.mfloorclip = ds->sprbottomclip;
    sprites.mceilingclip = ds->sprtopclip;

    // find positioning
    if (bsp.curline->linedef->flags & ML_DONTPEGBOTTOM)
    {
        draw.dc_texturemid =
            bsp.frontsector->floorheight > bsp.backsector->floorheight
                ? bsp.frontsector->floorheight
                : bsp.backsector->floorheight;
        draw.dc_texturemid = draw.dc_texturemid + textureheight[texnum] - pt.viewz;
    }
    else
    {
        draw.dc_texturemid =
            bsp.frontsector->ceilingheight < bsp.backsector->ceilingheight
                ? bsp.frontsector->ceilingheight
                : bsp.backsector->ceilingheight;
        draw.dc_texturemid = draw.dc_texturemid - pt.viewz;
    }
    draw.dc_texturemid += bsp.curline->sidedef->rowoffset;

    if (lights.fixedcolormap)
        draw.dc_colormap = lights.fixedcolormap;

    // draw the columns
    for (draw.dc_x = x1; draw.dc_x <= x2; draw.dc_x++)
    {
        // calculate lighting
        if (seg.maskedtexturecol[draw.dc_x] != DOOM_MAXSHORT)
        {
            if (!lights.fixedcolormap)
            {
                index = sprites.spryscale.raw >> LIGHTSCALESHIFT;

                if (index >= MAXLIGHTSCALE)
                    index = MAXLIGHTSCALE - 1;

                draw.dc_colormap = seg.walllights[index];
            }

            sprites.sprtopscreen = viewProjection().centeryfrac
                                   - FixedMul(draw.dc_texturemid, sprites.spryscale);
            draw.dc_iscale = fixed_t {static_cast<std::int32_t>(
                0xffffffffu / static_cast<unsigned>(sprites.spryscale.raw))};

            // draw the texture
            col = (Column*) ((byte*) Doom::getColumn(texnum,
                                                     seg.maskedtexturecol[draw.dc_x])
                             - 3);

            Doom::drawMaskedColumn(col);
            seg.maskedtexturecol[draw.dc_x] = DOOM_MAXSHORT;
        }
        sprites.spryscale += wall.rw_scalestep;
    }
}

//
// renderSegLoop
// Draws zero, one, or two textures (and possibly a masked
//  texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling
//  textures.
// CALLED: CORE LOOPING ROUTINE.
//
void renderSegLoop()
{
    auto& seg = segState();
    auto& draw = drawState();
    auto& plane = planeScratch();
    auto& scratch = renderScratch();
    auto& proj = viewProjection();
    auto& view = viewWindow();
    auto& wall = wallScratch();

    unsigned index;
    int yl;
    int yh;
    int mid;
    // Vanilla declares this fixed_t and shifts it down by fracBits in place; by the
    // time anything reads it, it is a whole texture column number.
    int texturecolumn;
    int top;
    int bottom;

    for (; seg.rw_x < seg.rw_stopx; seg.rw_x++)
    {
        // mark floor / ceiling areas
        yl = (wall.topfrac.raw + HEIGHTUNIT - 1) >> HEIGHTBITS;

        // no space above wall?
        if (yl < plane.ceilingclip[seg.rw_x] + 1)
            yl = plane.ceilingclip[seg.rw_x] + 1;

        if (seg.markceiling)
        {
            top = plane.ceilingclip[seg.rw_x] + 1;
            bottom = yl - 1;

            if (bottom >= plane.floorclip[seg.rw_x])
                bottom = plane.floorclip[seg.rw_x] - 1;

            if (top <= bottom)
            {
                scratch.ceilingplane->top[seg.rw_x] = top;
                scratch.ceilingplane->bottom[seg.rw_x] = bottom;
            }
        }

        yh = wall.bottomfrac.raw >> HEIGHTBITS;

        if (yh >= plane.floorclip[seg.rw_x])
            yh = plane.floorclip[seg.rw_x] - 1;

        if (seg.markfloor)
        {
            top = yh + 1;
            bottom = plane.floorclip[seg.rw_x] - 1;
            if (top <= plane.ceilingclip[seg.rw_x])
                top = plane.ceilingclip[seg.rw_x] + 1;
            if (top <= bottom)
            {
                scratch.floorplane->top[seg.rw_x] = top;
                scratch.floorplane->bottom[seg.rw_x] = bottom;
            }
        }

        // texturecolumn and lighting are independent of wall tiers
        if (seg.segtextured)
        {
            // calculate texture offset
            const auto angleFine =
                (wall.rw_centerangle + proj.xtoviewangle[seg.rw_x]).fineIndex();
            texturecolumn = (wall.rw_offset
                             - FixedMul(finetangent[angleFine], scratch.rw_distance))
                                .toInt();
            // calculate lighting
            index = wall.rw_scale.raw >> LIGHTSCALESHIFT;

            if (index >= MAXLIGHTSCALE)
                index = MAXLIGHTSCALE - 1;

            draw.dc_colormap = seg.walllights[index];
            draw.dc_x = seg.rw_x;
            draw.dc_iscale = fixed_t {static_cast<std::int32_t>(
                0xffffffffu / static_cast<unsigned>(wall.rw_scale.raw))};
        }

        // draw the wall tiers
        if (seg.midtexture)
        {
            // single sided line
            draw.dc_yl = yl;
            draw.dc_yh = yh;
            draw.dc_texturemid = wall.rw_midtexturemid;
            draw.dc_source = Doom::getColumn(seg.midtexture, texturecolumn);
            colfunc();
            plane.ceilingclip[seg.rw_x] = view.viewheight;
            plane.floorclip[seg.rw_x] = -1;
        }
        else
        {
            // two sided line
            if (seg.toptexture)
            {
                // top wall
                mid = wall.pixhigh.raw >> HEIGHTBITS;
                wall.pixhigh += wall.pixhighstep;

                if (mid >= plane.floorclip[seg.rw_x])
                    mid = plane.floorclip[seg.rw_x] - 1;

                if (mid >= yl)
                {
                    draw.dc_yl = yl;
                    draw.dc_yh = mid;
                    draw.dc_texturemid = wall.rw_toptexturemid;
                    draw.dc_source = Doom::getColumn(seg.toptexture, texturecolumn);
                    colfunc();
                    plane.ceilingclip[seg.rw_x] = mid;
                }
                else
                    plane.ceilingclip[seg.rw_x] = yl - 1;
            }
            else
            {
                // no top wall
                if (seg.markceiling)
                    plane.ceilingclip[seg.rw_x] = yl - 1;
            }

            if (seg.bottomtexture)
            {
                // bottom wall
                mid = (wall.pixlow.raw + HEIGHTUNIT - 1) >> HEIGHTBITS;
                wall.pixlow += wall.pixlowstep;

                // no space above wall?
                if (mid <= plane.ceilingclip[seg.rw_x])
                    mid = plane.ceilingclip[seg.rw_x] + 1;

                if (mid <= yh)
                {
                    draw.dc_yl = mid;
                    draw.dc_yh = yh;
                    draw.dc_texturemid = wall.rw_bottomtexturemid;
                    draw.dc_source =
                        Doom::getColumn(seg.bottomtexture, texturecolumn);
                    colfunc();
                    plane.floorclip[seg.rw_x] = mid;
                }
                else
                    plane.floorclip[seg.rw_x] = yh + 1;
            }
            else
            {
                // no bottom wall
                if (seg.markfloor)
                    plane.floorclip[seg.rw_x] = yh + 1;
            }

            if (wall.maskedtexture)
            {
                // save texturecol
                //  for backdrawing of masked mid texture
                seg.maskedtexturecol[seg.rw_x] = texturecolumn;
            }
        }

        wall.rw_scale += wall.rw_scalestep;
        wall.topfrac += wall.topstep;
        wall.bottomfrac += wall.bottomstep;
    }
}

//
// storeWallRange
// A wall segment will be drawn
//  between start and stop pixels (inclusive).
//
void storeWallRange(int start, int stop)
{
    auto& bsp = bspScratch();
    auto& seg = segState();
    auto& plane = planeScratch();
    auto& scratch = renderScratch();
    auto& lights = lighting();
    auto& sprites = spriteState();
    auto& pt = viewPoint();
    auto& proj = viewProjection();
    auto& sky = skyState();
    auto& wall = wallScratch();

    fixed_t hyp;
    fixed_t sineval;
    angle_t distangle, offsetangle;
    fixed_t vtop;
    int lightnum;

    // don't overflow and crash
    if (bsp.ds_p == &bsp.drawsegs[BSPScratch::maxDrawSegs])
        return;

#ifdef RANGECHECK
    if (start >= viewWindow().viewwidth || start > stop)
    {
        //fatalError("Error: Bad R_RenderWallRange: %i to %i", start, stop);

        doom_strcpy(error_buf, "Error: Bad R_RenderWallRange: ");
        doom_concat(error_buf, doom_itoa(start, 10));
        doom_concat(error_buf, " to ");
        doom_concat(error_buf, doom_itoa(stop, 10));
        fatalError(error_buf);
    }
#endif

    bsp.sidedef = bsp.curline->sidedef;
    bsp.linedef = bsp.curline->linedef;

    // mark the segment as visible for auto map
    bsp.linedef->flags |= ML_MAPPED;

    // calculate rw_distance for scale calculation
    scratch.rw_normalangle = bsp.curline->angle + ang90;
    offsetangle = scratch.rw_normalangle - scratch.rw_angle1;
    if (offsetangle > ang180)
        offsetangle = -offsetangle; // offsetangle = abs(rw_normalangle - rw_angle1)

    if (offsetangle > ang90)
        offsetangle = ang90;

    distangle = ang90 - offsetangle;
    hyp = Doom::pointToDist(bsp.curline->v1->x, bsp.curline->v1->y);
    sineval = finesine[distangle.fineIndex()];
    scratch.rw_distance = FixedMul(hyp, sineval);

    bsp.ds_p->x1 = seg.rw_x = start;
    bsp.ds_p->x2 = stop;
    bsp.ds_p->curline = bsp.curline;
    seg.rw_stopx = stop + 1;

    // calculate scale at both ends and step
    bsp.ds_p->scale1 = wall.rw_scale =
        Doom::scaleFromGlobalAngle(pt.viewangle + proj.xtoviewangle[start]);

    if (stop > start)
    {
        bsp.ds_p->scale2 =
            Doom::scaleFromGlobalAngle(pt.viewangle + proj.xtoviewangle[stop]);
        bsp.ds_p->scalestep = wall.rw_scalestep =
            (bsp.ds_p->scale2 - wall.rw_scale) / (stop - start);
    }
    else
    {
        // UNUSED: try to fix the stretched line bug
#if 0
        if (scratch.rw_distance < FRACUNIT / 2)
        {
            fixed_t                trx, try;
            fixed_t                gxt, gyt;

            trx = bsp.curline->v1->x - pt.viewx;
            try = bsp.curline->v1->y - pt.viewy;

            gxt = FixedMul(trx, pt.viewcos);
            gyt = -FixedMul(try, pt.viewsin);
            bsp.ds_p->scale1 = FixedDiv(proj.projection, gxt - gyt) << viewWindow().detailshift;
        }
#endif
        bsp.ds_p->scale2 = bsp.ds_p->scale1;
    }

    // calculate texture boundaries
    //  and decide if floor / ceiling marks are needed
    wall.worldtop = bsp.frontsector->ceilingheight - pt.viewz;
    wall.worldbottom = bsp.frontsector->floorheight - pt.viewz;

    seg.midtexture = seg.toptexture = seg.bottomtexture = wall.maskedtexture = 0;
    bsp.ds_p->maskedtexturecol = nullptr;

    if (!bsp.backsector)
    {
        // single sided line
        seg.midtexture = texturetranslation[bsp.sidedef->midtexture];
        // a single sided line is terminal, so it must mark ends
        seg.markfloor = seg.markceiling = true;
        if (bsp.linedef->flags & ML_DONTPEGBOTTOM)
        {
            vtop = bsp.frontsector->floorheight
                   + textureheight[bsp.sidedef->midtexture];
            // bottom of texture at bottom
            wall.rw_midtexturemid = vtop - pt.viewz;
        }
        else
        {
            // top of texture at top
            wall.rw_midtexturemid = wall.worldtop;
        }
        wall.rw_midtexturemid += bsp.sidedef->rowoffset;

        bsp.ds_p->silhouette = SIL_BOTH;
        bsp.ds_p->sprtopclip = sprites.screenheightarray.data();
        bsp.ds_p->sprbottomclip = sprites.negonearray.data();
        bsp.ds_p->bsilheight = fixed_t {DOOM_MAXINT};
        bsp.ds_p->tsilheight = fixed_t {DOOM_MININT};
    }
    else
    {
        // two sided line
        bsp.ds_p->sprtopclip = bsp.ds_p->sprbottomclip = nullptr;
        bsp.ds_p->silhouette = 0;

        if (bsp.frontsector->floorheight > bsp.backsector->floorheight)
        {
            bsp.ds_p->silhouette = SIL_BOTTOM;
            bsp.ds_p->bsilheight = bsp.frontsector->floorheight;
        }
        else if (bsp.backsector->floorheight > pt.viewz)
        {
            bsp.ds_p->silhouette = SIL_BOTTOM;
            bsp.ds_p->bsilheight = fixed_t {DOOM_MAXINT};
            // ds_p->sprbottomclip = negonearray;
        }

        if (bsp.frontsector->ceilingheight < bsp.backsector->ceilingheight)
        {
            bsp.ds_p->silhouette |= SIL_TOP;
            bsp.ds_p->tsilheight = bsp.frontsector->ceilingheight;
        }
        else if (bsp.backsector->ceilingheight < pt.viewz)
        {
            bsp.ds_p->silhouette |= SIL_TOP;
            bsp.ds_p->tsilheight = fixed_t {DOOM_MININT};
            // ds_p->sprtopclip = screenheightarray;
        }

        if (bsp.backsector->ceilingheight <= bsp.frontsector->floorheight)
        {
            bsp.ds_p->sprbottomclip = sprites.negonearray.data();
            bsp.ds_p->bsilheight = fixed_t {DOOM_MAXINT};
            bsp.ds_p->silhouette |= SIL_BOTTOM;
        }

        if (bsp.backsector->floorheight >= bsp.frontsector->ceilingheight)
        {
            bsp.ds_p->sprtopclip = sprites.screenheightarray.data();
            bsp.ds_p->tsilheight = fixed_t {DOOM_MININT};
            bsp.ds_p->silhouette |= SIL_TOP;
        }

        wall.worldhigh = bsp.backsector->ceilingheight - pt.viewz;
        wall.worldlow = bsp.backsector->floorheight - pt.viewz;

        // hack to allow height changes in outdoor areas
        if (bsp.frontsector->ceilingpic == sky.skyflatnum
            && bsp.backsector->ceilingpic == sky.skyflatnum)
        {
            wall.worldtop = wall.worldhigh;
        }

        if (wall.worldlow != wall.worldbottom
            || bsp.backsector->floorpic != bsp.frontsector->floorpic
            || bsp.backsector->lightlevel != bsp.frontsector->lightlevel)
        {
            seg.markfloor = true;
        }
        else
        {
            // same plane on both sides
            seg.markfloor = false;
        }

        if (wall.worldhigh != wall.worldtop
            || bsp.backsector->ceilingpic != bsp.frontsector->ceilingpic
            || bsp.backsector->lightlevel != bsp.frontsector->lightlevel)
        {
            seg.markceiling = true;
        }
        else
        {
            // same plane on both sides
            seg.markceiling = false;
        }

        if (bsp.backsector->ceilingheight <= bsp.frontsector->floorheight
            || bsp.backsector->floorheight >= bsp.frontsector->ceilingheight)
        {
            // closed door
            seg.markceiling = seg.markfloor = true;
        }

        if (wall.worldhigh < wall.worldtop)
        {
            // top texture
            seg.toptexture = texturetranslation[bsp.sidedef->toptexture];
            if (bsp.linedef->flags & ML_DONTPEGTOP)
            {
                // top of texture at top
                wall.rw_toptexturemid = wall.worldtop;
            }
            else
            {
                vtop = bsp.backsector->ceilingheight
                       + textureheight[bsp.sidedef->toptexture];

                // bottom of texture
                wall.rw_toptexturemid = vtop - pt.viewz;
            }
        }
        if (wall.worldlow > wall.worldbottom)
        {
            // bottom texture
            seg.bottomtexture = texturetranslation[bsp.sidedef->bottomtexture];

            if (bsp.linedef->flags & ML_DONTPEGBOTTOM)
            {
                // bottom of texture at bottom
                // top of texture at top
                wall.rw_bottomtexturemid = wall.worldtop;
            }
            else // top of texture at top
                wall.rw_bottomtexturemid = wall.worldlow;
        }
        wall.rw_toptexturemid += bsp.sidedef->rowoffset;
        wall.rw_bottomtexturemid += bsp.sidedef->rowoffset;

        // allocate space for masked texture tables
        if (bsp.sidedef->midtexture)
        {
            // masked midtexture
            wall.maskedtexture = true;
            bsp.ds_p->maskedtexturecol = seg.maskedtexturecol =
                plane.lastopening - seg.rw_x;
            plane.lastopening += seg.rw_stopx - seg.rw_x;
        }
    }

    // calculate rw_offset (only needed for textured lines)
    seg.segtextured =
        seg.midtexture | seg.toptexture | seg.bottomtexture | wall.maskedtexture;

    if (seg.segtextured)
    {
        offsetangle = scratch.rw_normalangle - scratch.rw_angle1;

        if (offsetangle > ang180)
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4146)
#endif
            offsetangle = -offsetangle;
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

        if (offsetangle > ang90)
            offsetangle = ang90;

        sineval = finesine[offsetangle.fineIndex()];
        wall.rw_offset = FixedMul(hyp, sineval);

        if (scratch.rw_normalangle - scratch.rw_angle1 < ang180)
            wall.rw_offset = -wall.rw_offset;

        wall.rw_offset += bsp.sidedef->textureoffset + bsp.curline->offset;
        wall.rw_centerangle = ang90 + pt.viewangle - scratch.rw_normalangle;

        // calculate light table
        //  use different light tables
        //  for horizontal / vertical / diagonal
        // OPTIMIZE: get rid of LIGHTSEGSHIFT globally
        if (!lights.fixedcolormap)
        {
            lightnum =
                (bsp.frontsector->lightlevel >> LIGHTSEGSHIFT) + lights.extralight;

            if (bsp.curline->v1->y == bsp.curline->v2->y)
                lightnum--;
            else if (bsp.curline->v1->x == bsp.curline->v2->x)
                lightnum++;

            if (lightnum < 0)
                seg.walllights = lights.scalelight[0].data();
            else if (lightnum >= LIGHTLEVELS)
                seg.walllights = lights.scalelight[LIGHTLEVELS - 1].data();
            else
                seg.walllights = lights.scalelight[lightnum].data();
        }
    }

    // if a floor / ceiling plane is on the wrong side
    //  of the view plane, it is definitely invisible
    //  and doesn't need to be marked.

    if (bsp.frontsector->floorheight >= pt.viewz)
    {
        // above view plane
        seg.markfloor = false;
    }

    if (bsp.frontsector->ceilingheight <= pt.viewz
        && bsp.frontsector->ceilingpic != sky.skyflatnum)
    {
        // below view plane
        seg.markceiling = false;
    }

    // calculate incremental stepping values for texture edges
    wall.worldtop >>= 4;
    wall.worldbottom >>= 4;

    wall.topstep = -FixedMul(wall.rw_scalestep, wall.worldtop);
    wall.topfrac = (proj.centeryfrac >> 4) - FixedMul(wall.worldtop, wall.rw_scale);

    wall.bottomstep = -FixedMul(wall.rw_scalestep, wall.worldbottom);
    wall.bottomfrac =
        (proj.centeryfrac >> 4) - FixedMul(wall.worldbottom, wall.rw_scale);

    if (bsp.backsector)
    {
        wall.worldhigh >>= 4;
        wall.worldlow >>= 4;

        if (wall.worldhigh < wall.worldtop)
        {
            wall.pixhigh =
                (proj.centeryfrac >> 4) - FixedMul(wall.worldhigh, wall.rw_scale);
            wall.pixhighstep = -FixedMul(wall.rw_scalestep, wall.worldhigh);
        }

        if (wall.worldlow > wall.worldbottom)
        {
            wall.pixlow =
                (proj.centeryfrac >> 4) - FixedMul(wall.worldlow, wall.rw_scale);
            wall.pixlowstep = -FixedMul(wall.rw_scalestep, wall.worldlow);
        }
    }

    // render it
    if (seg.markceiling)
        scratch.ceilingplane =
            Doom::checkPlane(scratch.ceilingplane, seg.rw_x, seg.rw_stopx - 1);

    if (seg.markfloor)
        scratch.floorplane =
            Doom::checkPlane(scratch.floorplane, seg.rw_x, seg.rw_stopx - 1);

    renderSegLoop();

    // save sprite clipping info
    if (((bsp.ds_p->silhouette & SIL_TOP) || wall.maskedtexture)
        && !bsp.ds_p->sprtopclip)
    {
        doom_memcpy(plane.lastopening,
                    plane.ceilingclip.data() + start,
                    2 * (seg.rw_stopx - start));
        bsp.ds_p->sprtopclip = plane.lastopening - start;
        plane.lastopening += seg.rw_stopx - start;
    }

    if (((bsp.ds_p->silhouette & SIL_BOTTOM) || wall.maskedtexture)
        && !bsp.ds_p->sprbottomclip)
    {
        doom_memcpy(plane.lastopening,
                    plane.floorclip.data() + start,
                    2 * (seg.rw_stopx - start));
        bsp.ds_p->sprbottomclip = plane.lastopening - start;
        plane.lastopening += seg.rw_stopx - start;
    }

    if (wall.maskedtexture && !(bsp.ds_p->silhouette & SIL_TOP))
    {
        bsp.ds_p->silhouette |= SIL_TOP;
        bsp.ds_p->tsilheight = fixed_t {DOOM_MININT};
    }
    if (wall.maskedtexture && !(bsp.ds_p->silhouette & SIL_BOTTOM))
    {
        bsp.ds_p->silhouette |= SIL_BOTTOM;
        bsp.ds_p->bsilheight = fixed_t {DOOM_MAXINT};
    }
    bsp.ds_p++;
}
} // namespace Doom
