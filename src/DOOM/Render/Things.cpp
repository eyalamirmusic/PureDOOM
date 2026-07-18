// Rewritten out of vanilla r_things into namespace Doom.
//
// Sprite / thing rendering: build the sprite-rotation tables at startup, project
// each thing in a sector to a vissprite, sort them back-to-front, and draw them
// (and the player's weapon psprites) clipped against the solid segs. r_things.cpp
// shims the R_ names and owns the vissprite pool + psprite-clip globals the other
// renderer files share; this unit's own scratch is file-local. Golden-neutral.

#include "../doom_config.h"

#include "../doomdef.h"
#include "../doomstat.h"
#include "../i_system.h"
#include "../m_swap.h"
#include "../r_local.h"
#include "../w_wad.h"

#include "GraphicsData.h"
#include "SpriteScratch.h"
#include "Things.h"

#include "Segs.h"
#include <ea_data_structures/Structures/Array.h>

#include "Draw.h"
#include "../Host/System.h"
#include "Main.h"
#define MINZ (FRACUNIT * 4)
#define BASEYCENTER 100

namespace Doom
{

// File-local scratch, now on the Engine (Render/SpriteScratch.h, moved by the file-scope-statics
// sweep - REFACTOR.md, Step 5). The vanilla names are references onto that member; no other file
// reads these.
static lighttable_t**& spritelights = spriteScratch().spritelights;

static spriteframe_t (&sprtemp)[29] = spriteScratch().sprtemp;
static int& maxframe = spriteScratch().maxframe;
static char*& spritename = spriteScratch().spritename;

static vissprite_t& overflowsprite = spriteScratch().overflowsprite;

// Forward declarations so call order needs no rearranging.
void installSpriteLump(int lump,
                       unsigned frame,
                       unsigned rotation,
                       doom_boolean flipped);
void initSpriteDefs(char** namelist);
void initSprites(char** namelist);
void clearSprites();
vissprite_t* newVisSprite();
void drawMaskedColumn(column_t* column);
void drawVisSprite(vissprite_t* vis);
void projectSprite(mobj_t* thing);
void addSprites(sector_t* sec);
void drawPSprite(pspdef_t* psp);
void drawPlayerSprites();
void sortVisSprites();
void drawSprite(vissprite_t* spr);
void drawMasked();

//
// installSpriteLump
// Local function for initSprites.
//
void installSpriteLump(int lump,
                       unsigned frame,
                       unsigned rotation,
                       doom_boolean flipped)
{
    if (frame >= 29 || rotation > 8)
    {
        doom_strcpy(error_buf,
                    "Error: R_InstallSpriteLump: Bad frame characters in lump ");
        doom_concat(error_buf, doom_itoa(lump, 10));
        fatalError(error_buf);
    }

    if (static_cast<int>(frame) > maxframe)
        maxframe = frame;

    if (rotation == 0)
    {
        // the lump should be used for all rotations
        if (sprtemp[frame].rotate == false)
        {
            doom_strcpy(error_buf, "Error: Doom::initSprites: Sprite ");
            doom_concat(error_buf, spritename);
            doom_concat(error_buf, " frame ");
            doom_concat(error_buf, doom_ctoa('A' + frame));
            doom_concat(error_buf, " has multip rot=0 lump");
            fatalError(error_buf);
        }

        if (sprtemp[frame].rotate == true)
        {
            doom_strcpy(error_buf, "Error: Doom::initSprites: Sprite ");
            doom_concat(error_buf, spritename);
            doom_concat(error_buf, " frame ");
            doom_concat(error_buf, doom_ctoa('A' + frame));
            doom_concat(error_buf, " has rotations ");
            fatalError(error_buf);
        }

        sprtemp[frame].rotate = false;
        for (int r = 0; r < 8; r++)
        {
            sprtemp[frame].lump[r] = lump - firstspritelump;
            sprtemp[frame].flip[r] = static_cast<byte>(flipped);
        }
        return;
    }

    // the lump is only used for one rotation
    if (sprtemp[frame].rotate == false)
    {
        doom_strcpy(error_buf, "Error: Doom::initSprites: Sprite ");
        doom_concat(error_buf, spritename);
        doom_concat(error_buf, " frame ");
        doom_concat(error_buf, doom_ctoa('A' + frame));
        doom_concat(error_buf, " has rotations ");
        fatalError(error_buf);
    }

    sprtemp[frame].rotate = true;

    // make 0 based
    rotation--;
    if (sprtemp[frame].lump[rotation] != -1)
    {
        doom_strcpy(error_buf, "Error: Doom::initSprites: Sprite ");
        doom_concat(error_buf, spritename);
        doom_concat(error_buf, " : ");
        doom_concat(error_buf, doom_ctoa('A' + frame));
        doom_concat(error_buf, " : ");
        doom_concat(error_buf, doom_ctoa('1' + rotation));
        doom_concat(error_buf, " ");
        fatalError(error_buf);
    }

    sprtemp[frame].lump[rotation] = lump - firstspritelump;
    sprtemp[frame].flip[rotation] = static_cast<byte>(flipped);
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
void initSpriteDefs(char** namelist)
{
    char** check;
    int i;
    int l;
    int intname;
    int frame;
    int rotation;
    int start;
    int end;
    int patched;

    // count the number of sprite names
    check = namelist;
    while (*check != nullptr)
        check++;

    numsprites = static_cast<int>(check - namelist);

    if (!numsprites)
        return;

    // GraphicsData owns the sprite table now (RAII, Step 9); sprites is a plain-pointer
    // view onto its data(), refreshed after the resize. The resize constructs each
    // spritedef_t (with an empty frames vector), which the loop below fills.
    auto& gd = graphicsData();
    gd.sprites.resize(numsprites);
    sprites = gd.sprites.data();

    start = firstspritelump - 1;
    end = lastspritelump + 1;

    // scan all the lump names for each of the names,
    //  noting the highest frame letter.
    // Just compare 4 characters as ints
    for (i = 0; i < numsprites; i++)
    {
        spritename = namelist[i];
        doom_memset(sprtemp, -1, sizeof(sprtemp));

        maxframe = -1;
        intname = *reinterpret_cast<int*>(namelist[i]);

        // scan the lumps,
        //  filling in the frames for whatever is found
        for (l = start + 1; l < end; l++)
        {
            if (*reinterpret_cast<int*>(lumpinfo[l].name) == intname)
            {
                frame = lumpinfo[l].name[4] - 'A';
                rotation = lumpinfo[l].name[5] - '0';

                if (modifiedgame)
                    patched = W_GetNumForName(lumpinfo[l].name);
                else
                    patched = l;

                installSpriteLump(patched, frame, rotation, false);

                if (lumpinfo[l].name[6])
                {
                    frame = lumpinfo[l].name[6] - 'A';
                    rotation = lumpinfo[l].name[7] - '0';
                    installSpriteLump(l, frame, rotation, true);
                }
            }
        }

        // check the frames that were found for completeness
        if (maxframe == -1)
        {
            sprites[i].numframes = 0;
            continue;
        }

        maxframe++;

        for (frame = 0; frame < maxframe; frame++)
        {
            switch (static_cast<int>(sprtemp[frame].rotate))
            {
                case -1:
                {
                    // no rotations were found for that frame at all
                    doom_strcpy(error_buf,
                                "Error: Doom::initSprites: No patches found for ");
                    doom_concat(error_buf, namelist[i]);
                    doom_concat(error_buf, " frame ");
                    doom_concat(error_buf, doom_ctoa(frame + 'A'));
                    fatalError(error_buf);
                    break;
                }

                case 0:
                    // only the first rotation is needed
                    break;

                case 1:
                    // must have all 8 frames
                    for (rotation = 0; rotation < 8; rotation++)
                        if (sprtemp[frame].lump[rotation] == -1)
                        {
                            doom_strcpy(error_buf, "Error: Doom::initSprites: Sprite ");
                            doom_concat(error_buf, namelist[i]);
                            doom_concat(error_buf, " frame ");
                            doom_concat(error_buf, doom_ctoa(frame + 'A'));
                            doom_concat(error_buf, " is missing rotations");
                            fatalError(error_buf);
                        }
                    break;
            }
        }

        // allocate space for the frames present and copy sprtemp to it. The frames
        // vector is RAII-owned by the spritedef_t now (Step 9); resize then copy the
        // POD sprtemp entries into its storage, as the malloc + memcpy did.
        sprites[i].numframes = maxframe;
        sprites[i].spriteframes.resize(maxframe);
        doom_memcpy(sprites[i].spriteframes.data(),
                    sprtemp,
                    maxframe * sizeof(spriteframe_t));
    }
}

//
// GAME FUNCTIONS
//

//
// initSprites
// Called at program start.
//
void initSprites(char** namelist)
{
    for (int i = 0; i < SCREENWIDTH; i++)
    {
        negonearray[i] = -1;
    }

    initSpriteDefs(namelist);
}

//
// clearSprites
// Called at frame start.
//
void clearSprites()
{
    vissprite_p = vissprites;
}

//
// newVisSprite
//
vissprite_t* newVisSprite()
{
    if (vissprite_p == &vissprites[MAXVISSPRITES])
        return &overflowsprite;

    vissprite_p++;
    return vissprite_p - 1;
}

//
// drawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//  in posts/runs of opaque pixels.
//
void drawMaskedColumn(column_t* column)
{
    int topscreen;
    int bottomscreen;
    fixed_t basetexturemid;

    basetexturemid = dc_texturemid;

    for (; column->topdelta != 0xff;)
    {
        // calculate unclipped screen coordinates
        //  for post
        topscreen = sprtopscreen + spryscale * column->topdelta;
        bottomscreen = topscreen + spryscale * column->length;

        dc_yl = (topscreen + FRACUNIT - 1) >> FRACBITS;
        dc_yh = (bottomscreen - 1) >> FRACBITS;

        if (dc_yh >= mfloorclip[dc_x])
            dc_yh = mfloorclip[dc_x] - 1;
        if (dc_yl <= mceilingclip[dc_x])
            dc_yl = mceilingclip[dc_x] + 1;

        if (dc_yl <= dc_yh)
        {
            dc_source = reinterpret_cast<byte*>(column) + 3;
            dc_texturemid = basetexturemid - (column->topdelta << FRACBITS);
            // dc_source = (byte *)column + 3 - column->topdelta;

            // Drawn by either Doom::drawColumn
            //  or (SHADOW) Doom::drawFuzzColumn.
            colfunc();
        }
        column = reinterpret_cast<column_t*>(reinterpret_cast<byte*>(column)
                                             + column->length + 4);
    }

    dc_texturemid = basetexturemid;
}

//
// drawVisSprite
//  mfloorclip and mceilingclip should also be set.
//
void drawVisSprite(vissprite_t* vis)
{
    column_t* column;
    int texturecolumn;
    fixed_t frac;
    patch_t* patch;

    patch = static_cast<patch_t*>(
        W_CacheLumpNum(vis->patch + firstspritelump, PU_CACHE));

    dc_colormap = vis->colormap;

    if (!dc_colormap)
    {
        // 0 colormap = shadow draw
        colfunc = fuzzcolfunc;
    }
    else if (vis->mobjflags & MF_TRANSLATION)
    {
        colfunc = Doom::drawTranslatedColumn;
        dc_translation =
            translationtables - 256
            + ((vis->mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT - 8));
    }

    dc_iscale = doom_abs(vis->xiscale) >> detailshift;
    dc_texturemid = vis->texturemid;
    frac = vis->startfrac;
    spryscale = vis->scale;
    sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);

    for (dc_x = vis->x1; dc_x <= vis->x2; dc_x++, frac += vis->xiscale)
    {
        texturecolumn = frac >> FRACBITS;
#ifdef RANGECHECK
        if (texturecolumn < 0 || texturecolumn >= SHORT(patch->width))
            fatalError("Error: R_DrawSpriteRange: bad texturecolumn");
#endif
        column = reinterpret_cast<column_t*>(
            reinterpret_cast<byte*>(patch) + LONG(patch->columnofs[texturecolumn]));
        drawMaskedColumn(column);
    }

    colfunc = basecolfunc;
}

//
// projectSprite
// Generates a vissprite for a thing
//  if it might be visible.
//
void projectSprite(mobj_t* thing)
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

    spritedef_t* sprdef;
    spriteframe_t* sprframe;
    int lump;

    unsigned rot;
    doom_boolean flip;

    int index;

    vissprite_t* vis;

    angle_t ang;
    fixed_t iscale;

    // transform the origin point
    tr_x = thing->x - viewx;
    tr_y = thing->y - viewy;

    gxt = FixedMul(tr_x, viewcos);
    gyt = -FixedMul(tr_y, viewsin);

    tz = gxt - gyt;

    // thing is behind view plane?
    if (tz < MINZ)
        return;

    xscale = FixedDiv(projection, tz);

    gxt = -FixedMul(tr_x, viewsin);
    gyt = FixedMul(tr_y, viewcos);
    tx = -(gyt + gxt);

    // too far off the side?
    if (doom_abs(tx) > (tz << 2))
        return;

    // decide which patch to use for sprite relative to player
#ifdef RANGECHECK
    if (static_cast<unsigned>(thing->sprite) >= static_cast<unsigned>(numsprites))
    {
        doom_strcpy(error_buf, "Error: R_ProjectSprite: invalid sprite number ");
        doom_concat(error_buf, doom_itoa(thing->sprite, 10));
        doom_concat(error_buf, " ");
        fatalError(error_buf);
    }
#endif
    sprdef = &sprites[thing->sprite];
#ifdef RANGECHECK
    if ((thing->frame & FF_FRAMEMASK) >= sprdef->numframes)
    {
        doom_strcpy(error_buf, "Error: R_ProjectSprite: invalid sprite frame ");
        doom_concat(error_buf, doom_itoa(thing->sprite, 10));
        doom_concat(error_buf, " : ");
        doom_concat(error_buf, doom_itoa(thing->frame, 10));
        doom_concat(error_buf, " ");
        fatalError(error_buf);
    }
#endif
    sprframe = &sprdef->spriteframes[thing->frame & FF_FRAMEMASK];

    if (sprframe->rotate)
    {
        // choose a different rotation based on player view
        ang = Doom::pointToAngle(thing->x, thing->y);
        rot = (ang - thing->angle + static_cast<unsigned>(ANG45 / 2) * 9) >> 29;
        lump = sprframe->lump[rot];
        flip = static_cast<doom_boolean>(sprframe->flip[rot]);
    }
    else
    {
        // use single rotation for all views
        lump = sprframe->lump[0];
        flip = static_cast<doom_boolean>(sprframe->flip[0]);
    }

    // calculate edges of the shape
    tx -= spriteoffset[lump];
    x1 = (centerxfrac + FixedMul(tx, xscale)) >> FRACBITS;

    // off the right side?
    if (x1 > viewwidth)
        return;

    tx += spritewidth[lump];
    x2 = ((centerxfrac + FixedMul(tx, xscale)) >> FRACBITS) - 1;

    // off the left side
    if (x2 < 0)
        return;

    // store information in a vissprite
    vis = newVisSprite();
    vis->mobjflags = thing->flags;
    vis->scale = xscale << detailshift;
    vis->gx = thing->x;
    vis->gy = thing->y;
    vis->gz = thing->z;
    vis->gzt = thing->z + spritetopoffset[lump];
    vis->texturemid = vis->gzt - viewz;
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= viewwidth ? viewwidth - 1 : x2;
    iscale = FixedDiv(FRACUNIT, xscale);

    if (flip)
    {
        vis->startfrac = spritewidth[lump] - 1;
        vis->xiscale = -iscale;
    }
    else
    {
        vis->startfrac = 0;
        vis->xiscale = iscale;
    }

    if (vis->x1 > x1)
        vis->startfrac += vis->xiscale * (vis->x1 - x1);
    vis->patch = lump;

    // get light level
    if (thing->flags & MF_SHADOW)
    {
        // shadow draw
        vis->colormap = nullptr;
    }
    else if (fixedcolormap)
    {
        // fixed map
        vis->colormap = fixedcolormap;
    }
    else if (thing->frame & FF_FULLBRIGHT)
    {
        // full bright
        vis->colormap = colormaps;
    }

    else
    {
        // diminished light
        index = xscale >> (LIGHTSCALESHIFT - detailshift);

        if (index >= MAXLIGHTSCALE)
            index = MAXLIGHTSCALE - 1;

        vis->colormap = spritelights[index];
    }
}

//
// addSprites
// During BSP traversal, this adds sprites by sector.
//
void addSprites(sector_t* sec)
{
    mobj_t* thing;
    int lightnum;

    // BSP is traversed by subsector.
    // A sector might have been split into several
    //  subsectors during BSP building.
    // Thus we check whether its already added.
    if (sec->validcount == validcount)
        return;

    // Well, now it will be done.
    sec->validcount = validcount;

    lightnum = (sec->lightlevel >> LIGHTSEGSHIFT) + extralight;

    if (lightnum < 0)
        spritelights = scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
        spritelights = scalelight[LIGHTLEVELS - 1];
    else
        spritelights = scalelight[lightnum];

    // Handle all things in sector.
    for (thing = sec->thinglist; thing; thing = thing->snext)
        projectSprite(thing);
}

//
// drawPSprite
//
void drawPSprite(pspdef_t* psp)
{
    fixed_t tx;
    int x1;
    int x2;
    spritedef_t* sprdef;
    spriteframe_t* sprframe;
    int lump;
    doom_boolean flip;
    vissprite_t* vis;
    vissprite_t avis;

    // decide which patch to use
#ifdef RANGECHECK
    if (static_cast<unsigned>(psp->state->sprite)
        >= static_cast<unsigned>(numsprites))
    {
        doom_strcpy(error_buf, "Error: R_ProjectSprite: invalid sprite number ");
        doom_concat(error_buf, doom_itoa(psp->state->sprite, 10));
        doom_concat(error_buf, " ");
        fatalError(error_buf);
    }
#endif
    sprdef = &sprites[psp->state->sprite];
#ifdef RANGECHECK
    if ((psp->state->frame & FF_FRAMEMASK) >= sprdef->numframes)
    {
        doom_strcpy(error_buf, "Error: R_ProjectSprite: invalid sprite frame ");
        doom_concat(error_buf, doom_itoa(psp->state->sprite, 10));
        doom_concat(error_buf, " : ");
        doom_concat(error_buf, doom_itoa(psp->state->frame, 10));
        doom_concat(error_buf, " ");
        fatalError(error_buf);
    }
#endif
    sprframe = &sprdef->spriteframes[psp->state->frame & FF_FRAMEMASK];

    lump = sprframe->lump[0];
    flip = static_cast<doom_boolean>(sprframe->flip[0]);

    // calculate edges of the shape
    tx = psp->sx - 160 * FRACUNIT;

    tx -= spriteoffset[lump];
    x1 = (centerxfrac + FixedMul(tx, pspritescale)) >> FRACBITS;

    // off the right side
    if (x1 > viewwidth)
        return;

    tx += spritewidth[lump];
    x2 = ((centerxfrac + FixedMul(tx, pspritescale)) >> FRACBITS) - 1;

    // off the left side
    if (x2 < 0)
        return;

    // store information in a vissprite
    vis = &avis;
    vis->mobjflags = 0;
    vis->texturemid =
        (BASEYCENTER << FRACBITS) + FRACUNIT / 2 - (psp->sy - spritetopoffset[lump]);
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= viewwidth ? viewwidth - 1 : x2;
    vis->scale = pspritescale << detailshift;

    if (flip)
    {
        vis->xiscale = -pspriteiscale;
        vis->startfrac = spritewidth[lump] - 1;
    }
    else
    {
        vis->xiscale = pspriteiscale;
        vis->startfrac = 0;
    }

    if (vis->x1 > x1)
        vis->startfrac += vis->xiscale * (vis->x1 - x1);

    vis->patch = lump;

    if (viewplayer->powers[pw_invisibility] > 4 * 32
        || viewplayer->powers[pw_invisibility] & 8)
    {
        // shadow draw
        vis->colormap = nullptr;
    }
    else if (fixedcolormap)
    {
        // fixed color
        vis->colormap = fixedcolormap;
    }
    else if (psp->state->frame & FF_FULLBRIGHT)
    {
        // full bright
        vis->colormap = colormaps;
    }
    else
    {
        // local light
        vis->colormap = spritelights[MAXLIGHTSCALE - 1];
    }

    drawVisSprite(vis);
}

//
// drawPlayerSprites
//
void drawPlayerSprites()
{
    int i;
    int lightnum;
    pspdef_t* psp;

    // get light level
    lightnum = (viewplayer->mo->subsector->sector->lightlevel >> LIGHTSEGSHIFT)
               + extralight;

    if (lightnum < 0)
        spritelights = scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
        spritelights = scalelight[LIGHTLEVELS - 1];
    else
        spritelights = scalelight[lightnum];

    // clip to screen bounds
    mfloorclip = screenheightarray;
    mceilingclip = negonearray;

    // add all active psprites
    for (i = 0, psp = viewplayer->psprites; i < NUMPSPRITES; i++, psp++)
    {
        if (psp->state)
            drawPSprite(psp);
    }
}

//
// sortVisSprites
//
void sortVisSprites()
{
    int count;
    vissprite_t* ds;
    vissprite_t* best = nullptr;
    vissprite_t unsorted;
    fixed_t bestscale;

    count = static_cast<int>(vissprite_p - vissprites);

    unsorted.next = unsorted.prev = &unsorted;

    if (!count)
        return;

    for (ds = vissprites; ds < vissprite_p; ds++)
    {
        ds->next = ds + 1;
        ds->prev = ds - 1;
    }

    vissprites[0].prev = &unsorted;
    unsorted.next = &vissprites[0];
    (vissprite_p - 1)->next = &unsorted;
    unsorted.prev = vissprite_p - 1;

    // pull the vissprites out by scale
    vsprsortedhead.next = vsprsortedhead.prev = &vsprsortedhead;
    for (int i = 0; i < count; i++)
    {
        bestscale = DOOM_MAXINT;
        for (ds = unsorted.next; ds != &unsorted; ds = ds->next)
        {
            if (ds->scale < bestscale)
            {
                bestscale = ds->scale;
                best = ds;
            }
        }
        best->next->prev = best->prev;
        best->prev->next = best->next;
        best->next = &vsprsortedhead;
        best->prev = vsprsortedhead.prev;
        vsprsortedhead.prev->next = best;
        vsprsortedhead.prev = best;
    }
}

//
// drawSprite
//
void drawSprite(vissprite_t* spr)
{
    drawseg_t* ds;
    EA::Array<short, SCREENWIDTH> clipbot;
    EA::Array<short, SCREENWIDTH> cliptop;
    int x;
    int r1;
    int r2;
    fixed_t scale;
    fixed_t lowscale;
    int silhouette;

    for (x = spr->x1; x <= spr->x2; x++)
        clipbot[x] = cliptop[x] = -2;

    // Scan drawsegs from end to start for obscuring segs.
    // The first drawseg that has a greater scale
    //  is the clip seg.
    for (ds = ds_p - 1; ds >= drawsegs; ds--)
    {
        // determine if the drawseg obscures the sprite
        if (ds->x1 > spr->x2 || ds->x2 < spr->x1
            || (!ds->silhouette && !ds->maskedtexturecol))
        {
            // does not cover sprite
            continue;
        }

        r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
        r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;

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

        if (scale < spr->scale
            || (lowscale < spr->scale
                && !Doom::pointOnSegSide(spr->gx, spr->gy, ds->curline)))
        {
            // masked mid texture?
            if (ds->maskedtexturecol)
                Doom::renderMaskedSegRange(ds, r1, r2);
            // seg is behind sprite
            continue;
        }

        // clip this piece of the sprite
        silhouette = ds->silhouette;

        if (spr->gz >= ds->bsilheight)
            silhouette &= ~SIL_BOTTOM;

        if (spr->gzt <= ds->tsilheight)
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
    for (x = spr->x1; x <= spr->x2; x++)
    {
        if (clipbot[x] == -2)
            clipbot[x] = viewheight;

        if (cliptop[x] == -2)
            cliptop[x] = -1;
    }

    mfloorclip = clipbot.data();
    mceilingclip = cliptop.data();
    drawVisSprite(spr);
}

//
// drawMasked
//
void drawMasked()
{
    vissprite_t* spr;
    drawseg_t* ds;

    sortVisSprites();

    if (vissprite_p > vissprites)
    {
        // draw all vissprites back to front
        for (spr = vsprsortedhead.next; spr != &vsprsortedhead; spr = spr->next)
        {
            drawSprite(spr);
        }
    }

    // render any remaining masked mid textures
    for (ds = ds_p - 1; ds >= drawsegs; ds--)
        if (ds->maskedtexturecol)
            Doom::renderMaskedSegRange(ds, ds->x1, ds->x2);

    // draw the psprites on top of everything
    drawPlayerSprites();
}

} // namespace Doom
