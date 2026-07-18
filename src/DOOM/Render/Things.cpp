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
#include <ea_data_structures/Structures/Array.h>

#include "Draw.h"
#include "../Host/System.h"
#include "Main.h"
#include "../Game/GameVersion.h"
#define MINZ (FRACUNIT * 4)
#define BASEYCENTER 100

namespace Doom
{

// File-local scratch, now on the Engine (Render/SpriteScratch.h, moved by the file-scope-statics
// sweep - REFACTOR.md, Step 5). The vanilla names are references onto that member; no other file
// reads these.
static LightTable**& spritelights = spriteScratch().spritelights;

static SpriteFrame (&sprtemp)[29] = spriteScratch().sprtemp;
static int& maxframe = spriteScratch().maxframe;
static char*& spritename = spriteScratch().spritename;

static VisSprite& overflowsprite = spriteScratch().overflowsprite;

// Forward declarations so call order needs no rearranging.
void installSpriteLump(int lump,
                       unsigned frame,
                       unsigned rotation,
                       doom_boolean flipped);
void initSpriteDefs(char** namelist);
void initSprites(char** namelist);
void clearSprites();
VisSprite* newVisSprite();
void drawMaskedColumn(Column* column);
void drawVisSprite(VisSprite* vis);
void projectSprite(Mobj* thing);
void addSprites(Sector* sec);
void drawPSprite(PspDef* psp);
void drawPlayerSprites();
void sortVisSprites();
void drawSprite(VisSprite* spr);
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
    auto& gd = graphicsData();

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
            sprtemp[frame].lump[r] = lump - gd.firstspritelump;
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

    sprtemp[frame].lump[rotation] = lump - gd.firstspritelump;
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

    auto& gd = graphicsData();

    // count the number of sprite names
    check = namelist;
    while (*check != nullptr)
        check++;

    gd.numsprites = static_cast<int>(check - namelist);

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
        spritename = namelist[i];
        doom_memset(sprtemp, -1, sizeof(sprtemp));

        maxframe = -1;
        intname = *reinterpret_cast<int*>(namelist[i]);

        // scan the lumps,
        //  filling in the frames for whatever is found
        for (l = start + 1; l < end; l++)
        {
            const Lump& entry = Doom::wad().info(l);

            if (*reinterpret_cast<const int*>(entry.name) == intname)
            {
                frame = entry.name[4] - 'A';
                rotation = entry.name[5] - '0';

                if (gameVersion().modifiedgame)
                    patched = Doom::wad().number(entry.name);
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
        // vector is RAII-owned by the SpriteDef now (Step 9); resize then copy the
        // POD sprtemp entries into its storage, as the malloc + memcpy did.
        sprites[i].numframes = maxframe;
        sprites[i].spriteframes.resize(maxframe);
        doom_memcpy(sprites[i].spriteframes.data(),
                    sprtemp,
                    maxframe * sizeof(SpriteFrame));
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
    auto& sprState = spriteState();

    for (int i = 0; i < SCREENWIDTH; i++)
    {
        sprState.negonearray[i] = -1;
    }

    initSpriteDefs(namelist);
}

//
// clearSprites
// Called at frame start.
//
void clearSprites()
{
    auto& sprState = spriteState();

    sprState.vissprite_p = sprState.vissprites;
}

//
// newVisSprite
//
VisSprite* newVisSprite()
{
    auto& sprState = spriteState();

    if (sprState.vissprite_p == &sprState.vissprites[MAXVISSPRITES])
        return &overflowsprite;

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
    int topscreen;
    int bottomscreen;
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

        draw.dc_yl = (topscreen + FRACUNIT - 1) >> FRACBITS;
        draw.dc_yh = (bottomscreen - 1) >> FRACBITS;

        if (draw.dc_yh >= sprState.mfloorclip[draw.dc_x])
            draw.dc_yh = sprState.mfloorclip[draw.dc_x] - 1;
        if (draw.dc_yl <= sprState.mceilingclip[draw.dc_x])
            draw.dc_yl = sprState.mceilingclip[draw.dc_x] + 1;

        if (draw.dc_yl <= draw.dc_yh)
        {
            draw.dc_source = reinterpret_cast<byte*>(column) + 3;
            draw.dc_texturemid = basetexturemid - (column->topdelta << FRACBITS);
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
void drawVisSprite(VisSprite* vis)
{
    Column* column;
    int texturecolumn;
    fixed_t frac;
    Patch* patch;

    auto& draw = drawState();
    auto& sprState = spriteState();

    patch = static_cast<Patch*>(
        Doom::cacheLumpNum(vis->patch + graphicsData().firstspritelump));

    draw.dc_colormap = vis->colormap;

    if (!draw.dc_colormap)
    {
        // 0 colormap = shadow draw
        colfunc = fuzzcolfunc;
    }
    else if (vis->mobjflags & MF_TRANSLATION)
    {
        colfunc = Doom::drawTranslatedColumn;
        draw.dc_translation =
            translationtables - 256
            + ((vis->mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT - 8));
    }

    draw.dc_iscale = doom_abs(vis->xiscale) >> viewWindow().detailshift;
    draw.dc_texturemid = vis->texturemid;
    frac = vis->startfrac;
    sprState.spryscale = vis->scale;
    sprState.sprtopscreen = viewProjection().centeryfrac
                            - FixedMul(draw.dc_texturemid, sprState.spryscale);

    for (draw.dc_x = vis->x1; draw.dc_x <= vis->x2;
         draw.dc_x++, frac += vis->xiscale)
    {
        texturecolumn = frac >> FRACBITS;
#ifdef RANGECHECK
        if (texturecolumn < 0 || texturecolumn >= SHORT(patch->width))
            fatalError("Error: R_DrawSpriteRange: bad texturecolumn");
#endif
        column = reinterpret_cast<Column*>(
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
void projectSprite(Mobj* thing)
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
    doom_boolean flip;

    int index;

    VisSprite* vis;

    angle_t ang;
    fixed_t iscale;

    auto& pt = viewPoint();
    auto& proj = viewProjection();
    auto& view = viewWindow();
    auto& lights = lighting();

    // transform the origin point
    tr_x = thing->x - pt.viewx;
    tr_y = thing->y - pt.viewy;

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
    if (static_cast<unsigned>(thing->sprite)
        >= static_cast<unsigned>(graphicsData().numsprites))
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
    x1 = (proj.centerxfrac + FixedMul(tx, xscale)) >> FRACBITS;

    // off the right side?
    if (x1 > view.viewwidth)
        return;

    tx += spritewidth[lump];
    x2 = ((proj.centerxfrac + FixedMul(tx, xscale)) >> FRACBITS) - 1;

    // off the left side
    if (x2 < 0)
        return;

    // store information in a vissprite
    vis = newVisSprite();
    vis->mobjflags = thing->flags;
    vis->scale = xscale << view.detailshift;
    vis->gx = thing->x;
    vis->gy = thing->y;
    vis->gz = thing->z;
    vis->gzt = thing->z + spritetopoffset[lump];
    vis->texturemid = vis->gzt - pt.viewz;
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= view.viewwidth ? view.viewwidth - 1 : x2;
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
    else if (lights.fixedcolormap)
    {
        // fixed map
        vis->colormap = lights.fixedcolormap;
    }
    else if (thing->frame & FF_FULLBRIGHT)
    {
        // full bright
        vis->colormap = colormaps;
    }

    else
    {
        // diminished light
        index = xscale >> (LIGHTSCALESHIFT - view.detailshift);

        if (index >= MAXLIGHTSCALE)
            index = MAXLIGHTSCALE - 1;

        vis->colormap = spritelights[index];
    }
}

//
// addSprites
// During BSP traversal, this adds sprites by sector.
//
void addSprites(Sector* sec)
{
    Mobj* thing;
    int lightnum;

    auto& valid = validCount();
    auto& lights = lighting();

    // BSP is traversed by subsector.
    // A sector might have been split into several
    //  subsectors during BSP building.
    // Thus we check whether its already added.
    if (sec->validcount == valid.validcount)
        return;

    // Well, now it will be done.
    sec->validcount = valid.validcount;

    lightnum = (sec->lightlevel >> LIGHTSEGSHIFT) + lights.extralight;

    if (lightnum < 0)
        spritelights = lights.scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
        spritelights = lights.scalelight[LIGHTLEVELS - 1];
    else
        spritelights = lights.scalelight[lightnum];

    // Handle all things in sector.
    for (thing = sec->thinglist; thing; thing = thing->snext)
        projectSprite(thing);
}

//
// drawPSprite
//
void drawPSprite(PspDef* psp)
{
    fixed_t tx;
    int x1;
    int x2;
    SpriteDef* sprdef;
    SpriteFrame* sprframe;
    int lump;
    doom_boolean flip;
    VisSprite* vis;
    VisSprite avis;

    auto& pt = viewPoint();
    auto& proj = viewProjection();
    auto& view = viewWindow();
    auto& lights = lighting();
    auto& sprState = spriteState();

    // decide which patch to use
#ifdef RANGECHECK
    if (static_cast<unsigned>(psp->state->sprite)
        >= static_cast<unsigned>(graphicsData().numsprites))
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
    x1 = (proj.centerxfrac + FixedMul(tx, sprState.pspritescale)) >> FRACBITS;

    // off the right side
    if (x1 > view.viewwidth)
        return;

    tx += spritewidth[lump];
    x2 = ((proj.centerxfrac + FixedMul(tx, sprState.pspritescale)) >> FRACBITS) - 1;

    // off the left side
    if (x2 < 0)
        return;

    // store information in a vissprite
    vis = &avis;
    vis->mobjflags = 0;
    vis->texturemid =
        (BASEYCENTER << FRACBITS) + FRACUNIT / 2 - (psp->sy - spritetopoffset[lump]);
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= view.viewwidth ? view.viewwidth - 1 : x2;
    vis->scale = sprState.pspritescale << view.detailshift;

    if (flip)
    {
        vis->xiscale = -sprState.pspriteiscale;
        vis->startfrac = spritewidth[lump] - 1;
    }
    else
    {
        vis->xiscale = sprState.pspriteiscale;
        vis->startfrac = 0;
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
    PspDef* psp;

    auto& pt = viewPoint();
    auto& lights = lighting();
    auto& sprState = spriteState();

    // get light level
    lightnum = (pt.viewplayer->mo->subsector->sector->lightlevel >> LIGHTSEGSHIFT)
               + lights.extralight;

    if (lightnum < 0)
        spritelights = lights.scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
        spritelights = lights.scalelight[LIGHTLEVELS - 1];
    else
        spritelights = lights.scalelight[lightnum];

    // clip to screen bounds
    sprState.mfloorclip = sprState.screenheightarray;
    sprState.mceilingclip = sprState.negonearray;

    // add all active psprites
    for (i = 0, psp = pt.viewplayer->psprites; i < NUMPSPRITES; i++, psp++)
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
    VisSprite* ds;
    VisSprite* best = nullptr;
    VisSprite unsorted;
    fixed_t bestscale;

    auto& sprState = spriteState();

    count = static_cast<int>(sprState.vissprite_p - sprState.vissprites);

    unsorted.next = unsorted.prev = &unsorted;

    if (!count)
        return;

    for (ds = sprState.vissprites; ds < sprState.vissprite_p; ds++)
    {
        ds->next = ds + 1;
        ds->prev = ds - 1;
    }

    sprState.vissprites[0].prev = &unsorted;
    unsorted.next = &sprState.vissprites[0];
    (sprState.vissprite_p - 1)->next = &unsorted;
    unsorted.prev = sprState.vissprite_p - 1;

    // pull the vissprites out by scale
    sprState.vsprsortedhead.next = sprState.vsprsortedhead.prev =
        &sprState.vsprsortedhead;
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
        best->next = &sprState.vsprsortedhead;
        best->prev = sprState.vsprsortedhead.prev;
        sprState.vsprsortedhead.prev->next = best;
        sprState.vsprsortedhead.prev = best;
    }
}

//
// drawSprite
//
void drawSprite(VisSprite* spr)
{
    DrawSeg* ds;
    EA::Array<short, SCREENWIDTH> clipbot;
    EA::Array<short, SCREENWIDTH> cliptop;
    int x;
    int r1;
    int r2;
    fixed_t scale;
    fixed_t lowscale;
    int silhouette;

    auto& bsp = bspScratch();
    auto& sprState = spriteState();
    auto& view = viewWindow();

    for (x = spr->x1; x <= spr->x2; x++)
        clipbot[x] = cliptop[x] = -2;

    // Scan drawsegs from end to start for obscuring segs.
    // The first drawseg that has a greater scale
    //  is the clip seg.
    for (ds = bsp.ds_p - 1; ds >= bsp.drawsegs; ds--)
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

    if (sprState.vissprite_p > sprState.vissprites)
    {
        // draw all vissprites back to front
        for (spr = sprState.vsprsortedhead.next; spr != &sprState.vsprsortedhead;
             spr = spr->next)
        {
            drawSprite(spr);
        }
    }

    // render any remaining masked mid textures
    for (ds = bsp.ds_p - 1; ds >= bsp.drawsegs; ds--)
        if (ds->maskedtexturecol)
            Doom::renderMaskedSegRange(ds, ds->x1, ds->x2);

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
//  and sprites is a plain-pointer view onto its owned EA::Vector, set by
//  R_InitSpriteDefs after the fill (Step 9).
Doom::SpriteDef* sprites = nullptr;

// The vissprite pool and its sorted list head (read by r_segs).

// The masked-column clip windows and sprite scale (read by r_segs).
