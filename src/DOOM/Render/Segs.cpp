// Rewritten out of vanilla r_segs into namespace Doom.
//
// Wall (seg) rendering: draw a wall range's textured columns, the masked mid-texture
// pass for two-sided lines, and store a visible wall range for later. r_segs.cpp
// shims the R_ names and owns the per-column wall state r_bsp/r_plane share; its own
// scratch is file-local. Golden-neutral.

#include "../doom_config.h"

#include "../doomdef.h"
#include "../doomstat.h"
#include "../i_system.h"
#include "../r_local.h"

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
#define HEIGHTBITS 12
#define HEIGHTUNIT (1 << HEIGHTBITS)

// These are Doom::SegState members (Engine), exported by the r_segs.cpp shim; references onto
// them - and as references, not plain externs, since Render/Segs writes them (a write through a
// plain extern would clobber the reference's pointer, the skytexturemid trap).

namespace Doom
{

// The per-wall-segment rendering intermediates now live on the Engine (Render/WallScratch.h, moved
// by the file-scope-statics sweep - REFACTOR.md, Step 5). The vanilla names are references onto that
// member; read by no other file.
static doom_boolean& maskedtexture = wallScratch().maskedtexture;

static angle_t& rw_centerangle = wallScratch().rw_centerangle;
static fixed_t& rw_offset = wallScratch().rw_offset;
static fixed_t& rw_scale = wallScratch().rw_scale;
static fixed_t& rw_scalestep = wallScratch().rw_scalestep;
static fixed_t& rw_midtexturemid = wallScratch().rw_midtexturemid;
static fixed_t& rw_toptexturemid = wallScratch().rw_toptexturemid;
static fixed_t& rw_bottomtexturemid = wallScratch().rw_bottomtexturemid;

static int& worldtop = wallScratch().worldtop;
static int& worldbottom = wallScratch().worldbottom;
static int& worldhigh = wallScratch().worldhigh;
static int& worldlow = wallScratch().worldlow;

static fixed_t& pixhigh = wallScratch().pixhigh;
static fixed_t& pixlow = wallScratch().pixlow;
static fixed_t& pixhighstep = wallScratch().pixhighstep;
static fixed_t& pixlowstep = wallScratch().pixlowstep;
static fixed_t& topfrac = wallScratch().topfrac;
static fixed_t& topstep = wallScratch().topstep;
static fixed_t& bottomfrac = wallScratch().bottomfrac;
static fixed_t& bottomstep = wallScratch().bottomstep;

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
        seg.walllights = lights.scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
        seg.walllights = lights.scalelight[LIGHTLEVELS - 1];
    else
        seg.walllights = lights.scalelight[lightnum];

    seg.maskedtexturecol = ds->maskedtexturecol;

    rw_scalestep = ds->scalestep;
    sprites.spryscale = ds->scale1 + (x1 - ds->x1) * rw_scalestep;
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
                index = sprites.spryscale >> LIGHTSCALESHIFT;

                if (index >= MAXLIGHTSCALE)
                    index = MAXLIGHTSCALE - 1;

                draw.dc_colormap = seg.walllights[index];
            }

            sprites.sprtopscreen = viewProjection().centeryfrac
                                   - FixedMul(draw.dc_texturemid, sprites.spryscale);
            draw.dc_iscale = 0xffffffffu / static_cast<unsigned>(sprites.spryscale);

            // draw the texture
            col = (Column*) ((byte*) Doom::getColumn(texnum,
                                                     seg.maskedtexturecol[draw.dc_x])
                             - 3);

            Doom::drawMaskedColumn(col);
            seg.maskedtexturecol[draw.dc_x] = DOOM_MAXSHORT;
        }
        sprites.spryscale += rw_scalestep;
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

    angle_t angle;
    unsigned index;
    int yl;
    int yh;
    int mid;
    fixed_t texturecolumn;
    int top;
    int bottom;

    for (; seg.rw_x < seg.rw_stopx; seg.rw_x++)
    {
        // mark floor / ceiling areas
        yl = (topfrac + HEIGHTUNIT - 1) >> HEIGHTBITS;

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

        yh = bottomfrac >> HEIGHTBITS;

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
            angle =
                (rw_centerangle + proj.xtoviewangle[seg.rw_x]) >> ANGLETOFINESHIFT;
            texturecolumn =
                rw_offset - FixedMul(finetangent[angle], scratch.rw_distance);
            texturecolumn >>= FRACBITS;
            // calculate lighting
            index = rw_scale >> LIGHTSCALESHIFT;

            if (index >= MAXLIGHTSCALE)
                index = MAXLIGHTSCALE - 1;

            draw.dc_colormap = seg.walllights[index];
            draw.dc_x = seg.rw_x;
            draw.dc_iscale = 0xffffffffu / static_cast<unsigned>(rw_scale);
        }

        // draw the wall tiers
        if (seg.midtexture)
        {
            // single sided line
            draw.dc_yl = yl;
            draw.dc_yh = yh;
            draw.dc_texturemid = rw_midtexturemid;
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
                mid = pixhigh >> HEIGHTBITS;
                pixhigh += pixhighstep;

                if (mid >= plane.floorclip[seg.rw_x])
                    mid = plane.floorclip[seg.rw_x] - 1;

                if (mid >= yl)
                {
                    draw.dc_yl = yl;
                    draw.dc_yh = mid;
                    draw.dc_texturemid = rw_toptexturemid;
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
                mid = (pixlow + HEIGHTUNIT - 1) >> HEIGHTBITS;
                pixlow += pixlowstep;

                // no space above wall?
                if (mid <= plane.ceilingclip[seg.rw_x])
                    mid = plane.ceilingclip[seg.rw_x] + 1;

                if (mid <= yh)
                {
                    draw.dc_yl = mid;
                    draw.dc_yh = yh;
                    draw.dc_texturemid = rw_bottomtexturemid;
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

            if (maskedtexture)
            {
                // save texturecol
                //  for backdrawing of masked mid texture
                seg.maskedtexturecol[seg.rw_x] = texturecolumn;
            }
        }

        rw_scale += rw_scalestep;
        topfrac += topstep;
        bottomfrac += bottomstep;
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

    fixed_t hyp;
    fixed_t sineval;
    angle_t distangle, offsetangle;
    fixed_t vtop;
    int lightnum;

    // don't overflow and crash
    if (bsp.ds_p == &bsp.drawsegs[MAXDRAWSEGS])
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
    scratch.rw_normalangle = bsp.curline->angle + ANG90;
    offsetangle = scratch.rw_normalangle - scratch.rw_angle1;
    if (offsetangle > ANG180)
        offsetangle = -offsetangle; // offsetangle = abs(rw_normalangle - rw_angle1)

    if (offsetangle > ANG90)
        offsetangle = ANG90;

    distangle = ANG90 - offsetangle;
    hyp = Doom::pointToDist(bsp.curline->v1->x, bsp.curline->v1->y);
    sineval = finesine[distangle >> ANGLETOFINESHIFT];
    scratch.rw_distance = FixedMul(hyp, sineval);

    bsp.ds_p->x1 = seg.rw_x = start;
    bsp.ds_p->x2 = stop;
    bsp.ds_p->curline = bsp.curline;
    seg.rw_stopx = stop + 1;

    // calculate scale at both ends and step
    bsp.ds_p->scale1 = rw_scale =
        Doom::scaleFromGlobalAngle(pt.viewangle + proj.xtoviewangle[start]);

    if (stop > start)
    {
        bsp.ds_p->scale2 =
            Doom::scaleFromGlobalAngle(pt.viewangle + proj.xtoviewangle[stop]);
        bsp.ds_p->scalestep = rw_scalestep =
            (bsp.ds_p->scale2 - rw_scale) / (stop - start);
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
    worldtop = bsp.frontsector->ceilingheight - pt.viewz;
    worldbottom = bsp.frontsector->floorheight - pt.viewz;

    seg.midtexture = seg.toptexture = seg.bottomtexture = maskedtexture = 0;
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
            rw_midtexturemid = vtop - pt.viewz;
        }
        else
        {
            // top of texture at top
            rw_midtexturemid = worldtop;
        }
        rw_midtexturemid += bsp.sidedef->rowoffset;

        bsp.ds_p->silhouette = SIL_BOTH;
        bsp.ds_p->sprtopclip = sprites.screenheightarray;
        bsp.ds_p->sprbottomclip = sprites.negonearray;
        bsp.ds_p->bsilheight = DOOM_MAXINT;
        bsp.ds_p->tsilheight = DOOM_MININT;
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
            bsp.ds_p->bsilheight = DOOM_MAXINT;
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
            bsp.ds_p->tsilheight = DOOM_MININT;
            // ds_p->sprtopclip = screenheightarray;
        }

        if (bsp.backsector->ceilingheight <= bsp.frontsector->floorheight)
        {
            bsp.ds_p->sprbottomclip = sprites.negonearray;
            bsp.ds_p->bsilheight = DOOM_MAXINT;
            bsp.ds_p->silhouette |= SIL_BOTTOM;
        }

        if (bsp.backsector->floorheight >= bsp.frontsector->ceilingheight)
        {
            bsp.ds_p->sprtopclip = sprites.screenheightarray;
            bsp.ds_p->tsilheight = DOOM_MININT;
            bsp.ds_p->silhouette |= SIL_TOP;
        }

        worldhigh = bsp.backsector->ceilingheight - pt.viewz;
        worldlow = bsp.backsector->floorheight - pt.viewz;

        // hack to allow height changes in outdoor areas
        if (bsp.frontsector->ceilingpic == sky.skyflatnum
            && bsp.backsector->ceilingpic == sky.skyflatnum)
        {
            worldtop = worldhigh;
        }

        if (worldlow != worldbottom
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

        if (worldhigh != worldtop
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

        if (worldhigh < worldtop)
        {
            // top texture
            seg.toptexture = texturetranslation[bsp.sidedef->toptexture];
            if (bsp.linedef->flags & ML_DONTPEGTOP)
            {
                // top of texture at top
                rw_toptexturemid = worldtop;
            }
            else
            {
                vtop = bsp.backsector->ceilingheight
                       + textureheight[bsp.sidedef->toptexture];

                // bottom of texture
                rw_toptexturemid = vtop - pt.viewz;
            }
        }
        if (worldlow > worldbottom)
        {
            // bottom texture
            seg.bottomtexture = texturetranslation[bsp.sidedef->bottomtexture];

            if (bsp.linedef->flags & ML_DONTPEGBOTTOM)
            {
                // bottom of texture at bottom
                // top of texture at top
                rw_bottomtexturemid = worldtop;
            }
            else // top of texture at top
                rw_bottomtexturemid = worldlow;
        }
        rw_toptexturemid += bsp.sidedef->rowoffset;
        rw_bottomtexturemid += bsp.sidedef->rowoffset;

        // allocate space for masked texture tables
        if (bsp.sidedef->midtexture)
        {
            // masked midtexture
            maskedtexture = true;
            bsp.ds_p->maskedtexturecol = seg.maskedtexturecol =
                plane.lastopening - seg.rw_x;
            plane.lastopening += seg.rw_stopx - seg.rw_x;
        }
    }

    // calculate rw_offset (only needed for textured lines)
    seg.segtextured =
        seg.midtexture | seg.toptexture | seg.bottomtexture | maskedtexture;

    if (seg.segtextured)
    {
        offsetangle = scratch.rw_normalangle - scratch.rw_angle1;

        if (offsetangle > ANG180)
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4146)
#endif
            offsetangle = -offsetangle;
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

        if (offsetangle > ANG90)
            offsetangle = ANG90;

        sineval = finesine[offsetangle >> ANGLETOFINESHIFT];
        rw_offset = FixedMul(hyp, sineval);

        if (scratch.rw_normalangle - scratch.rw_angle1 < ANG180)
            rw_offset = -rw_offset;

        rw_offset += bsp.sidedef->textureoffset + bsp.curline->offset;
        rw_centerangle = ANG90 + pt.viewangle - scratch.rw_normalangle;

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
                seg.walllights = lights.scalelight[0];
            else if (lightnum >= LIGHTLEVELS)
                seg.walllights = lights.scalelight[LIGHTLEVELS - 1];
            else
                seg.walllights = lights.scalelight[lightnum];
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
    worldtop >>= 4;
    worldbottom >>= 4;

    topstep = -FixedMul(rw_scalestep, worldtop);
    topfrac = (proj.centeryfrac >> 4) - FixedMul(worldtop, rw_scale);

    bottomstep = -FixedMul(rw_scalestep, worldbottom);
    bottomfrac = (proj.centeryfrac >> 4) - FixedMul(worldbottom, rw_scale);

    if (bsp.backsector)
    {
        worldhigh >>= 4;
        worldlow >>= 4;

        if (worldhigh < worldtop)
        {
            pixhigh = (proj.centeryfrac >> 4) - FixedMul(worldhigh, rw_scale);
            pixhighstep = -FixedMul(rw_scalestep, worldhigh);
        }

        if (worldlow > worldbottom)
        {
            pixlow = (proj.centeryfrac >> 4) - FixedMul(worldlow, rw_scale);
            pixlowstep = -FixedMul(rw_scalestep, worldlow);
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
    if (((bsp.ds_p->silhouette & SIL_TOP) || maskedtexture) && !bsp.ds_p->sprtopclip)
    {
        doom_memcpy(plane.lastopening,
                    plane.ceilingclip + start,
                    2 * (seg.rw_stopx - start));
        bsp.ds_p->sprtopclip = plane.lastopening - start;
        plane.lastopening += seg.rw_stopx - start;
    }

    if (((bsp.ds_p->silhouette & SIL_BOTTOM) || maskedtexture)
        && !bsp.ds_p->sprbottomclip)
    {
        doom_memcpy(
            plane.lastopening, plane.floorclip + start, 2 * (seg.rw_stopx - start));
        bsp.ds_p->sprbottomclip = plane.lastopening - start;
        plane.lastopening += seg.rw_stopx - start;
    }

    if (maskedtexture && !(bsp.ds_p->silhouette & SIL_TOP))
    {
        bsp.ds_p->silhouette |= SIL_TOP;
        bsp.ds_p->tsilheight = DOOM_MININT;
    }
    if (maskedtexture && !(bsp.ds_p->silhouette & SIL_BOTTOM))
    {
        bsp.ds_p->silhouette |= SIL_BOTTOM;
        bsp.ds_p->bsilheight = DOOM_MAXINT;
    }
    bsp.ds_p++;
}
} // namespace Doom
