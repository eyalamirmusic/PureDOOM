// Rewritten out of vanilla r_things into namespace Doom.
//
// Sprite / thing rendering: build the sprite-rotation tables at startup, project
// each thing in a sector to a vissprite, sort them back-to-front, and draw them
// (and the player's weapon psprites) clipped against the solid segs. r_things.cpp
// shims the R_ names and owns the vissprite pool + psprite-clip globals the other
// renderer files share; this unit's own scratch is file-local. Golden-neutral.

#include "../Host/Platform.h"

#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"
#include "../Math/Swap.h"
#include "../Wad/WadFile.h"

#include "../Sim/ValidCount.h"
#include "BSPScratch.h"
#include "DrawState.h"
#include "GraphicsData.h"
#include "Lighting.h"
#include "SpriteScratch.h"
#include "SpriteState.h"
#include "Things.h"
#include "ViewPoint.h"
#include "ViewProjection.h"
#include "ViewWindow.h"

#include "Segs.h"
#include "../Containers.h"

#include "Draw.h"
#include "../Host/System.h"
#include "Main.h"
#include "../Game/GameVersion.h"

#include <algorithm>

namespace Doom
{

constexpr fixed_t MINZ = FRACUNIT * 4;
constexpr int BASEYCENTER = 100;

// File-local scratch, now on the Engine (Render/SpriteScratch.h, moved by the file-scope-statics
// sweep - REFACTOR.md, Step 5); no other file reads these. sprtemp, spritelights, maxframe,
// spritename and overflowsprite were all references onto that member too until the file-local-alias
// sweep (REFACTOR.md, Step 9 strand (a)) retired them - each toucher below hoists spriteScratch()
// and reaches them through it.

// Forward declarations so call order needs no rearranging.
void installSpriteLump(int lump, unsigned frame, unsigned rotation, bool flipped);
void initSpriteDefs(std::span<const std::string_view> namelist);
void initSprites(std::span<const std::string_view> namelist);
void clearSprites();
VisSprite* newVisSprite();
void drawMaskedColumn(Column* column);
void drawVisSprite(VisSprite& vis);
void projectSprite(Mobj& thing);
void addSprites(Sector& sec);
void drawPSprite(PspDef& psp);
void drawPlayerSprites();
void sortVisSprites();
void drawSprite(VisSprite& spr);
void drawMasked();

//
// installSpriteLump
// Local function for initSprites.
//
void installSpriteLump(int lump, unsigned frame, unsigned rotation, bool flipped)
{
    auto& gd = graphicsData();
    auto& scratch = spriteScratch();

    if (frame >= 29 || rotation > 8)
    {
        fatalError("Error: R_InstallSpriteLump: Bad frame characters in lump ",
                   lump);
    }

    scratch.maxframe = std::max(scratch.maxframe, static_cast<int>(frame));

    if (rotation == 0)
    {
        // the lump should be used for all rotations
        if (scratch.sprtemp[frame].rotate == singleRotation)
        {
            fatalError("Error: Doom::initSprites: Sprite ",
                       scratch.spritename,
                       " frame ",
                       char('A' + frame),
                       " has multip rot=0 lump");
        }

        if (scratch.sprtemp[frame].rotate == eightRotations)
        {
            fatalError("Error: Doom::initSprites: Sprite ",
                       scratch.spritename,
                       " frame ",
                       char('A' + frame),
                       " has rotations ");
        }

        scratch.sprtemp[frame].rotate = singleRotation;
        for (int r = 0; r < 8; r++)
        {
            scratch.sprtemp[frame].lump[r] = lump - gd.firstspritelump;
            scratch.sprtemp[frame].flip[r] = static_cast<byte>(flipped);
        }
        return;
    }

    // the lump is only used for one rotation
    if (scratch.sprtemp[frame].rotate == singleRotation)
    {
        fatalError("Error: Doom::initSprites: Sprite ",
                   scratch.spritename,
                   " frame ",
                   char('A' + frame),
                   " has rotations ");
    }

    scratch.sprtemp[frame].rotate = eightRotations;

    // make 0 based
    rotation--;
    if (scratch.sprtemp[frame].lump[rotation] != -1)
    {
        fatalError("Error: Doom::initSprites: Sprite ",
                   scratch.spritename,
                   " : ",
                   char('A' + frame),
                   " : ",
                   char('1' + rotation),
                   " ");
    }

    scratch.sprtemp[frame].lump[rotation] = lump - gd.firstspritelump;
    scratch.sprtemp[frame].flip[rotation] = static_cast<byte>(flipped);
}

//
// initSpriteDefs
// Pass a null terminated list of sprite names
//  (4 chars exactly) to be used.
// Builds the sprite rotation matrixes to account
//  for horizontally flipped sprites.
// Will report an error if the lumps are inconsistant.
// Only called at startup.
//
// Sprite lump names are 4 characters for the actor,
//  a letter for the frame, and a number for the rotation.
// A sprite that is flippable will have an additional
//  letter/number appended.
// The rotation character can be 0 to signify no rotations.
//
void initSpriteDefs(std::span<const std::string_view> namelist)
{
    int i;
    int l;
    int intname;
    int frame;
    int rotation;
    int start;
    int end;
    int patched;

    auto& gd = graphicsData();
    auto& scratch = spriteScratch();

    gd.numsprites = static_cast<int>(namelist.size());

    if (!gd.numsprites)
        return;

    // GraphicsData owns the sprite table now (RAII, Step 9); sprites is a plain-pointer
    // view onto its data(), refreshed after the resize. The resize constructs each
    // SpriteDef (with an empty frames vector), which the loop below fills.
    gd.sprites.resize(gd.numsprites);
    sprites = gd.sprites.data();

    start = gd.firstspritelump - 1;
    end = gd.lastspritelump + 1;

    // scan all the lump names for each of the names,
    //  noting the highest frame letter.
    // Just compare 4 characters as ints
    for (i = 0; i < gd.numsprites; i++)
    {
        scratch.spritename = namelist[i];
        doom_memset(scratch.sprtemp.data(), -1, sizeof(scratch.sprtemp));

        scratch.maxframe = -1;
        intname = *reinterpret_cast<const int*>(namelist[i].data());

        // scan the lumps,
        //  filling in the frames for whatever is found
        for (l = start + 1; l < end; l++)
        {
            const Lump& entry = Doom::wad().info(l);

            if (*reinterpret_cast<const int*>(entry.name.data()) == intname)
            {
                frame = entry.name[4] - 'A';
                rotation = entry.name[5] - '0';

                if (gameVersion().modifiedgame)
                    patched = Doom::wad().number(nameView(entry.name.data(), 8));
                else
                    patched = l;

                installSpriteLump(patched, frame, rotation, false);

                if (entry.name[6])
                {
                    frame = entry.name[6] - 'A';
                    rotation = entry.name[7] - '0';
                    installSpriteLump(l, frame, rotation, true);
                }
            }
        }

        // check the frames that were found for completeness
        if (scratch.maxframe == -1)
        {
            sprites[i].numframes = 0;
            continue;
        }

        scratch.maxframe++;

        for (frame = 0; frame < scratch.maxframe; frame++)
        {
            switch (static_cast<int>(scratch.sprtemp[frame].rotate))
            {
                case noRotationsSeen:
                {
                    // no rotations were found for that frame at all
                    fatalError("Error: Doom::initSprites: No patches found for ",
                               namelist[i],
                               " frame ",
                               char(frame + 'A'));
                    break;
                }

                case 0:
                    // only the first rotation is needed
                    break;

                case 1:
                    // must have all 8 frames
                    for (short lump: scratch.sprtemp[frame].lump)
                        if (lump == -1)
                        {
                            fatalError("Error: Doom::initSprites: Sprite ",
                                       namelist[i],
                                       " frame ",
                                       char(frame + 'A'),
                                       " is missing rotations");
                        }
                    break;
            }
        }

        // allocate space for the frames present and copy sprtemp to it. The frames
        // vector is RAII-owned by the SpriteDef now (Step 9); resize then copy the
        // POD sprtemp entries into its storage, as the malloc + memcpy did.
        sprites[i].numframes = scratch.maxframe;
        sprites[i].spriteframes.resize(scratch.maxframe);
        doom_memcpy(sprites[i].spriteframes.data(),
                    scratch.sprtemp.data(),
                    scratch.maxframe * sizeof(SpriteFrame));
    }
}

//
// GAME FUNCTIONS
//

//
// initSprites
// Called at program start.
//
void initSprites(std::span<const std::string_view> namelist)
{
    auto& sprState = spriteState();

    sprState.negonearray.fill(-1);

    initSpriteDefs(namelist);
}

//
// clearSprites
// Called at frame start.
//
void clearSprites()
{
    auto& sprState = spriteState();

    sprState.vissprite_p = sprState.vissprites.data();
}

//
// newVisSprite
//
VisSprite* newVisSprite()
{
    auto& sprState = spriteState();

    // data() + N rather than &vissprites[N] - the one-past-the-end address, which
    // is an out-of-range subscript on the Array (std::array) this now is, and
    // which MSVC's debug STL asserts on. See the same change in Sim/Mobj.cpp.
    if (sprState.vissprite_p
        == sprState.vissprites.data() + SpriteState::maxVisSprites)
        return &spriteScratch().overflowsprite;

    sprState.vissprite_p++;
    return sprState.vissprite_p - 1;
}

//
// drawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//  in posts/runs of opaque pixels.
//
void drawMaskedColumn(Column* column)
{
    // Vanilla declares these int, but they are fixed-point screen positions
    // (sprtopscreen and spryscale are both fixed_t).
    fixed_t topscreen;
    fixed_t bottomscreen;
    fixed_t basetexturemid;

    auto& draw = drawState();
    auto& sprState = spriteState();

    basetexturemid = draw.dc_texturemid;

    for (; column->topdelta != 0xff;)
    {
        // calculate unclipped screen coordinates
        //  for post
        topscreen = sprState.sprtopscreen + sprState.spryscale * column->topdelta;
        bottomscreen = topscreen + sprState.spryscale * column->length;

        draw.dc_yl = (topscreen.raw + fracUnit - 1) >> fracBits;
        draw.dc_yh = (bottomscreen.raw - 1) >> fracBits;

        if (draw.dc_yh >= sprState.mfloorclip[draw.dc_x])
            draw.dc_yh = sprState.mfloorclip[draw.dc_x] - 1;
        if (draw.dc_yl <= sprState.mceilingclip[draw.dc_x])
            draw.dc_yl = sprState.mceilingclip[draw.dc_x] + 1;

        if (draw.dc_yl <= draw.dc_yh)
        {
            draw.dc_source = reinterpret_cast<byte*>(column) + 3;
            draw.dc_texturemid =
                basetexturemid - Doom::Fixed::fromInt(column->topdelta);
            // dc_source = (byte *)column + 3 - column->topdelta;

            // Drawn by either Doom::drawColumn
            //  or (SHADOW) Doom::drawFuzzColumn.
            colfunc();
        }
        column = reinterpret_cast<Column*>(reinterpret_cast<byte*>(column)
                                           + column->length + 4);
    }

    draw.dc_texturemid = basetexturemid;
}

//
// drawVisSprite
//  mfloorclip and mceilingclip should also be set.
//
void drawVisSprite(VisSprite& vis)
{
    Column* column;
    int texturecolumn;
    fixed_t frac;
    Patch* patch;

    auto& draw = drawState();
    auto& sprState = spriteState();

    patch = static_cast<Patch*>(
        Doom::cacheLumpNum(vis.patch + graphicsData().firstspritelump));

    draw.dc_colormap = vis.colormap;

    if (!draw.dc_colormap)
    {
        // 0 colormap = shadow draw
        colfunc = fuzzcolfunc;
    }
    else if (vis.mobjflags & MF_TRANSLATION)
    {
        colfunc = Doom::drawTranslatedColumn;
        draw.dc_translation =
            translationtables - 256
            + ((vis.mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT - 8));
    }

    draw.dc_iscale = doom_abs(vis.xiscale) >> viewWindow().detailshift;
    draw.dc_texturemid = vis.texturemid;
    frac = vis.startfrac;
    sprState.spryscale = vis.scale;
    sprState.sprtopscreen = viewProjection().centeryfrac
                            - FixedMul(draw.dc_texturemid, sprState.spryscale);

    for (draw.dc_x = vis.x1; draw.dc_x <= vis.x2; draw.dc_x++, frac += vis.xiscale)
    {
        texturecolumn = frac.toInt();
#ifdef RANGECHECK
        if (texturecolumn < 0 || texturecolumn >= littleEndian(patch->width))
            fatalError("Error: R_DrawSpriteRange: bad texturecolumn");
#endif
        column = reinterpret_cast<Column*>(
            reinterpret_cast<byte*>(patch)
            + littleEndian(patch->columnofs[texturecolumn]));
        drawMaskedColumn(column);
    }

    colfunc = basecolfunc;
}

//
// projectSprite
// Generates a vissprite for a thing
//  if it might be visible.
//
void projectSprite(Mobj& thing)
{
    fixed_t tr_x;
    fixed_t tr_y;

    fixed_t gxt;
    fixed_t gyt;

    fixed_t tx;
    fixed_t tz;

    fixed_t xscale;

    int x1;
    int x2;

    SpriteDef* sprdef;
    SpriteFrame* sprframe;
    int lump;

    unsigned rot;
    bool flip;

    int index;

    VisSprite* vis;

    angle_t ang;
    fixed_t iscale;

    auto& pt = viewPoint();
    auto& proj = viewProjection();
    auto& view = viewWindow();
    auto& lights = lighting();

    // transform the origin point
    tr_x = thing.x - pt.viewx;
    tr_y = thing.y - pt.viewy;

    gxt = FixedMul(tr_x, pt.viewcos);
    gyt = -FixedMul(tr_y, pt.viewsin);

    tz = gxt - gyt;

    // thing is behind view plane?
    if (tz < MINZ)
        return;

    xscale = FixedDiv(proj.projection, tz);

    gxt = -FixedMul(tr_x, pt.viewsin);
    gyt = FixedMul(tr_y, pt.viewcos);
    tx = -(gyt + gxt);

    // too far off the side?
    if (doom_abs(tx) > (tz << 2))
        return;

    // decide which patch to use for sprite relative to player
#ifdef RANGECHECK
    if (static_cast<unsigned>(thing.sprite)
        >= static_cast<unsigned>(graphicsData().numsprites))
    {
        fatalError("Error: R_ProjectSprite: invalid sprite number ",
                   static_cast<int>(thing.sprite),
                   " ");
    }
#endif
    sprdef = &sprites[thing.sprite];
#ifdef RANGECHECK
    if ((thing.frame & FF_FRAMEMASK) >= sprdef->numframes)
    {
        fatalError("Error: R_ProjectSprite: invalid sprite frame ",
                   static_cast<int>(thing.sprite),
                   " : ",
                   thing.frame,
                   " ");
    }
#endif
    sprframe = &sprdef->spriteframes[thing.frame & FF_FRAMEMASK];

    if (sprframe->rotate)
    {
        // choose a different rotation based on player view
        ang = Doom::pointToAngle(thing.x, thing.y);
        rot = ((ang - thing.angle + (ang45 / 2u) * 9u) >> 29).raw;
        lump = sprframe->lump[rot];
        flip = static_cast<bool>(sprframe->flip[rot]);
    }
    else
    {
        // use single rotation for all views
        lump = sprframe->lump[0];
        flip = static_cast<bool>(sprframe->flip[0]);
    }

    // calculate edges of the shape
    tx -= spriteoffset[lump];
    x1 = (proj.centerxfrac + FixedMul(tx, xscale)).toInt();

    // off the right side?
    if (x1 > view.viewwidth)
        return;

    tx += spritewidth[lump];
    x2 = (proj.centerxfrac + FixedMul(tx, xscale)).toInt() - 1;

    // off the left side
    if (x2 < 0)
        return;

    // store information in a vissprite
    vis = newVisSprite();
    vis->mobjflags = thing.flags;
    vis->scale = xscale << view.detailshift;
    vis->gx = thing.x;
    vis->gy = thing.y;
    vis->gz = thing.z;
    vis->gzt = thing.z + spritetopoffset[lump];
    vis->texturemid = vis->gzt - pt.viewz;
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= view.viewwidth ? view.viewwidth - 1 : x2;
    iscale = FixedDiv(FRACUNIT, xscale);

    if (flip)
    {
        vis->startfrac = spritewidth[lump] - fixed_t {1};
        vis->xiscale = -iscale;
    }
    else
    {
        vis->startfrac = fixed_t {};
        vis->xiscale = iscale;
    }

    if (vis->x1 > x1)
        vis->startfrac += vis->xiscale * (vis->x1 - x1);
    vis->patch = lump;

    // get light level
    if (thing.flags & MF_SHADOW)
    {
        // shadow draw
        vis->colormap = nullptr;
    }
    else if (lights.fixedcolormap)
    {
        // fixed map
        vis->colormap = lights.fixedcolormap;
    }
    else if (thing.frame & FF_FULLBRIGHT)
    {
        // full bright
        vis->colormap = colormaps;
    }

    else
    {
        // diminished light
        index = xscale.raw >> (LIGHTSCALESHIFT - view.detailshift);

        index = std::min(index, MAXLIGHTSCALE - 1);

        vis->colormap = spriteScratch().spritelights[index];
    }
}

//
// addSprites
// During BSP traversal, this adds sprites by sector.
//
void addSprites(Sector& sec)
{
    Mobj* thing;
    int lightnum;

    auto& valid = validCount();
    auto& lights = lighting();
    auto& scratch = spriteScratch();

    // BSP is traversed by subsector.
    // A sector might have been split into several
    //  subsectors during BSP building.
    // Thus we check whether its already added.
    if (sec.validcount == valid.validcount)
        return;

    // Well, now it will be done.
    sec.validcount = valid.validcount;

    lightnum = (sec.lightlevel >> LIGHTSEGSHIFT) + lights.extralight;

    if (lightnum < 0)
        scratch.spritelights = lights.scalelight[0].data();
    else if (lightnum >= LIGHTLEVELS)
        scratch.spritelights = lights.scalelight[LIGHTLEVELS - 1].data();
    else
        scratch.spritelights = lights.scalelight[lightnum].data();

    // Handle all things in sector.
    for (thing = sec.thinglist; thing; thing = thing->snext)
        projectSprite(*thing);
}

//
// drawPSprite
//
void drawPSprite(PspDef& psp)
{
    fixed_t tx;
    int x1;
    int x2;
    SpriteDef* sprdef;
    SpriteFrame* sprframe;
    int lump;
    bool flip;
    VisSprite* vis;
    VisSprite avis;

    auto& pt = viewPoint();
    auto& proj = viewProjection();
    auto& view = viewWindow();
    auto& lights = lighting();
    auto& sprState = spriteState();

    // decide which patch to use
#ifdef RANGECHECK
    if (static_cast<unsigned>(psp.state->sprite)
        >= static_cast<unsigned>(graphicsData().numsprites))
    {
        fatalError("Error: R_ProjectSprite: invalid sprite number ",
                   static_cast<int>(psp.state->sprite),
                   " ");
    }
#endif
    sprdef = &sprites[psp.state->sprite];
#ifdef RANGECHECK
    if ((psp.state->frame & FF_FRAMEMASK) >= sprdef->numframes)
    {
        fatalError("Error: R_ProjectSprite: invalid sprite frame ",
                   static_cast<int>(psp.state->sprite),
                   " : ",
                   psp.state->frame,
                   " ");
    }
#endif
    sprframe = &sprdef->spriteframes[psp.state->frame & FF_FRAMEMASK];

    lump = sprframe->lump[0];
    flip = static_cast<bool>(sprframe->flip[0]);

    // calculate edges of the shape
    tx = psp.sx - 160 * FRACUNIT;

    tx -= spriteoffset[lump];
    x1 = (proj.centerxfrac + FixedMul(tx, sprState.pspritescale)).toInt();

    // off the right side
    if (x1 > view.viewwidth)
        return;

    tx += spritewidth[lump];
    x2 = (proj.centerxfrac + FixedMul(tx, sprState.pspritescale)).toInt() - 1;

    // off the left side
    if (x2 < 0)
        return;

    // store information in a vissprite
    vis = &avis;
    vis->mobjflags = 0;
    vis->texturemid = Doom::Fixed::fromInt(BASEYCENTER) + FRACUNIT / 2
                      - (psp.sy - spritetopoffset[lump]);
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= view.viewwidth ? view.viewwidth - 1 : x2;
    vis->scale = sprState.pspritescale << view.detailshift;

    if (flip)
    {
        vis->xiscale = -sprState.pspriteiscale;
        vis->startfrac = spritewidth[lump] - fixed_t {1};
    }
    else
    {
        vis->xiscale = sprState.pspriteiscale;
        vis->startfrac = fixed_t {};
    }

    if (vis->x1 > x1)
        vis->startfrac += vis->xiscale * (vis->x1 - x1);

    vis->patch = lump;

    if (pt.viewplayer->powers[pw_invisibility] > 4 * 32
        || pt.viewplayer->powers[pw_invisibility] & 8)
    {
        // shadow draw
        vis->colormap = nullptr;
    }
    else if (lights.fixedcolormap)
    {
        // fixed color
        vis->colormap = lights.fixedcolormap;
    }
    else if (psp.state->frame & FF_FULLBRIGHT)
    {
        // full bright
        vis->colormap = colormaps;
    }
    else
    {
        // local light
        vis->colormap = spriteScratch().spritelights[MAXLIGHTSCALE - 1];
    }

    drawVisSprite(*vis);
}

//
// drawPlayerSprites
//
void drawPlayerSprites()
{
    int i;
    int lightnum;
    PspDef* psp;

    auto& pt = viewPoint();
    auto& lights = lighting();
    auto& sprState = spriteState();
    auto& scratch = spriteScratch();

    // get light level
    lightnum = (pt.viewplayer->mo->subsector->sector->lightlevel >> LIGHTSEGSHIFT)
               + lights.extralight;

    if (lightnum < 0)
        scratch.spritelights = lights.scalelight[0].data();
    else if (lightnum >= LIGHTLEVELS)
        scratch.spritelights = lights.scalelight[LIGHTLEVELS - 1].data();
    else
        scratch.spritelights = lights.scalelight[lightnum].data();

    // clip to screen bounds
    sprState.mfloorclip = sprState.screenheightarray.data();
    sprState.mceilingclip = sprState.negonearray.data();

    // add all active psprites
    for (i = 0, psp = pt.viewplayer->psprites; i < NUMPSPRITES; i++, psp++)
    {
        if (psp->state)
            drawPSprite(*psp);
    }
}

//
// sortVisSprites
//
void sortVisSprites()
{
    auto& sprState = spriteState();
    auto& head = sprState.vsprsortedhead;

    head.next = head.prev = &head;

    if (sprState.vissprite_p == sprState.vissprites.data())
        return;

    // Ascending by scale, so drawMasked's forward walk paints far to near.
    //
    // stable_sort, not sort, and that is load-bearing rather than fastidious.
    // Vanilla selection-sorted with a strict <, so a later sprite at exactly the
    // same scale never displaced the incumbent and equal-depth sprites kept the
    // order they were added to vissprites in. Which of an equal-depth pair paints
    // over the other is a visible pixel.
    //
    // Measured, not assumed: swapping this for std::sort moves the *frame* golden
    // at demo2 tic 412 and demo3 tic 232, and leaves the simulation hashes
    // untouched. Equal fixed-point scales are common enough to hit twice in three
    // demos.
    std::stable_sort(sprState.vissprites.data(),
                     sprState.vissprite_p,
                     [](const VisSprite& a, const VisSprite& b)
                     { return a.scale < b.scale; });

    for (auto* sprite = sprState.vissprites.data(); sprite < sprState.vissprite_p;
         sprite++)
    {
        sprite->next = &head;
        sprite->prev = head.prev;
        head.prev->next = sprite;
        head.prev = sprite;
    }
}

//
// drawSprite
//
void drawSprite(VisSprite& spr)
{
    DrawSeg* ds;
    Array<short, SCREENWIDTH> clipbot;
    Array<short, SCREENWIDTH> cliptop;
    int x;
    int r1;
    int r2;
    fixed_t scale;
    fixed_t lowscale;
    int silhouette;

    auto& bsp = bspScratch();
    auto& sprState = spriteState();
    auto& view = viewWindow();

    for (x = spr.x1; x <= spr.x2; x++)
        clipbot[x] = cliptop[x] = -2;

    // Scan drawsegs from end to start for obscuring segs.
    // The first drawseg that has a greater scale
    //  is the clip seg.
    for (ds = bsp.ds_p - 1; ds >= bsp.drawsegs.data(); ds--)
    {
        // determine if the drawseg obscures the sprite
        if (ds->x1 > spr.x2 || ds->x2 < spr.x1
            || (!ds->silhouette && !ds->maskedtexturecol))
        {
            // does not cover sprite
            continue;
        }

        r1 = ds->x1 < spr.x1 ? spr.x1 : ds->x1;
        r2 = ds->x2 > spr.x2 ? spr.x2 : ds->x2;

        if (ds->scale1 > ds->scale2)
        {
            lowscale = ds->scale2;
            scale = ds->scale1;
        }
        else
        {
            lowscale = ds->scale1;
            scale = ds->scale2;
        }

        if (scale < spr.scale
            || (lowscale < spr.scale
                && !Doom::pointOnSegSide(spr.gx, spr.gy, *ds->curline)))
        {
            // masked mid texture?
            if (ds->maskedtexturecol)
                Doom::renderMaskedSegRange(*ds, r1, r2);
            // seg is behind sprite
            continue;
        }

        // clip this piece of the sprite
        silhouette = ds->silhouette;

        if (spr.gz >= ds->bsilheight)
            silhouette &= ~SIL_BOTTOM;

        if (spr.gzt <= ds->tsilheight)
            silhouette &= ~SIL_TOP;

        if (silhouette == 1)
        {
            // bottom sil
            for (x = r1; x <= r2; x++)
                if (clipbot[x] == -2)
                    clipbot[x] = ds->sprbottomclip[x];
        }
        else if (silhouette == 2)
        {
            // top sil
            for (x = r1; x <= r2; x++)
                if (cliptop[x] == -2)
                    cliptop[x] = ds->sprtopclip[x];
        }
        else if (silhouette == 3)
        {
            // both
            for (x = r1; x <= r2; x++)
            {
                if (clipbot[x] == -2)
                    clipbot[x] = ds->sprbottomclip[x];
                if (cliptop[x] == -2)
                    cliptop[x] = ds->sprtopclip[x];
            }
        }
    }

    // all clipping has been performed, so draw the sprite

    // check for unclipped columns
    for (x = spr.x1; x <= spr.x2; x++)
    {
        if (clipbot[x] == -2)
            clipbot[x] = view.viewheight;

        if (cliptop[x] == -2)
            cliptop[x] = -1;
    }

    sprState.mfloorclip = clipbot.data();
    sprState.mceilingclip = cliptop.data();
    drawVisSprite(spr);
}

//
// drawMasked
//
void drawMasked()
{
    VisSprite* spr;
    DrawSeg* ds;

    auto& bsp = bspScratch();
    auto& sprState = spriteState();

    sortVisSprites();

    if (sprState.vissprite_p > sprState.vissprites.data())
    {
        // draw all vissprites back to front
        for (spr = sprState.vsprsortedhead.next; spr != &sprState.vsprsortedhead;
             spr = spr->next)
        {
            drawSprite(*spr);
        }
    }

    // render any remaining masked mid textures
    for (ds = bsp.ds_p - 1; ds >= bsp.drawsegs.data(); ds--)
        if (ds->maskedtexturecol)
            Doom::renderMaskedSegRange(*ds, ds->x1, ds->x2);

    // draw the psprites on top of everything
    drawPlayerSprites();
}

} // namespace Doom

// ---------------------------------------------------------------------------
// Global-scope data that was r_things.cpp. It stays at :: scope because these are the
// vanilla names other translation units (and the eacp port) still link against.
// ---------------------------------------------------------------------------
// The sprite-drawing state r_things exports is a Doom::SpriteState owned by the Engine now; the
// definitions below are references onto its members (REFACTOR.md, Step 5).

// Sprite scaling for the player's own weapon sprites (read by r_main/r_plane).

// Constant arrays used for psprite clipping and initializing clipping
//  (read by r_segs/r_main).

// Variables used to look up and range check thing_t sprites patches
//  (read across the renderer and the app). The sprite frame table lives in
//  Doom::GraphicsData (an Engine member) now; numsprites is a reference onto it,
//  and sprites is a plain-pointer view onto its owned Vector, set by
//  R_InitSpriteDefs after the fill (Step 9).
Doom::SpriteDef* sprites = nullptr;

// The vissprite pool and its sorted list head (read by r_segs).

// The masked-column clip windows and sprite scale (read by r_segs).
