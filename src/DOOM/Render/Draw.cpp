// Rewritten out of vanilla r_draw into namespace Doom.
//
// The actual column and span blitting: the wall/sprite column drawers, the
// spectre fuzz and translated-colour variants, the flat span drawers, and the
// view-buffer / border setup they all draw through. r_draw.cpp shims the R_ names
// (r_main stores those shim addresses in colfunc/spanfunc/...) and owns the
// drawer input state (dc_*, ds_*) the other renderer files fill in; the frame
// address tables and the fuzz walk are file-local here. Golden-neutral.

#include "../doom_config.h"

#include "../doomdef.h"
#include "../doomstat.h" // State.
#include "../i_system.h"
#include "../r_local.h"
#include "../v_video.h" // Needs access to LFB, V_DrawPatch, V_MarkRect.
#include "../w_wad.h"

#include "Draw.h"
#include "DrawTables.h"

// ?
#define MAXWIDTH 1120
#define MAXHEIGHT 832

// status bar height at bottom of screen
#define SBARHEIGHT 32

#define FUZZTABLE 50
#define FUZZOFF (SCREENWIDTH)

namespace Doom
{

//
// All drawing to the view buffer is accomplished in this file. The other refresh
// files only know about coordinates, not the architecture of the frame buffer.
// Conveniently, the frame buffer is a linear one, and we need only the base
// address, and the total size == width*height*depth/8.
//
// These frame-address lookup tables (and fuzzpos below) now live on the Engine (Render/DrawTables.h,
// moved by the file-scope-statics sweep - REFACTOR.md, Step 5). The vanilla names are references onto
// that member; only the drawers use them, through R_InitBuffer.
static byte* (&ylookup)[MAXHEIGHT] = drawTables().ylookup;
static int (&columnofs)[MAXWIDTH] = drawTables().columnofs;

//
// A column is a vertical slice/span from a wall texture that, given the DOOM
// style restrictions on the view orientation, will always have constant z depth.
// Thus a special case loop for very fast rendering can be used. It has also been
// used with Wolfenstein 3D.
//
void drawColumn()
{
    int count;
    byte* dest;
    fixed_t frac;
    fixed_t fracstep;

    count = dc_yh - dc_yl;

    // Zero length, column does not exceed a pixel.
    if (count < 0)
        return;

#ifdef RANGECHECK
    if (static_cast<unsigned>(dc_x) >= SCREENWIDTH || dc_yl < 0
        || dc_yh >= SCREENHEIGHT)
    {
        doom_strcpy(error_buf, "Error: R_DrawColumn: ");
        doom_concat(error_buf, doom_itoa(dc_yl, 10));
        doom_concat(error_buf, " to ");
        doom_concat(error_buf, doom_itoa(dc_yh, 10));
        doom_concat(error_buf, " at ");
        doom_concat(error_buf, doom_itoa(dc_x, 10));
        I_Error(error_buf);
    }
#endif

    // Framebuffer destination address.
    // Use ylookup LUT to avoid multiply with ScreenWidth.
    // Use columnofs LUT for subwindows?
    dest = ylookup[dc_yl] + columnofs[dc_x];

    // Determine scaling,
    //  which is the only mapping to be done.
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.
    do
    {
        // Re-map color indices from wall texture column
        //  using a lighting/special effects LUT.
        *dest = dc_colormap[dc_source[(frac >> FRACBITS) & 127]];

        dest += SCREENWIDTH;
        frac += fracstep;

    } while (count--);
}

void drawColumnLow()
{
    int count;
    byte* dest;
    byte* dest2;
    fixed_t frac;
    fixed_t fracstep;

    count = dc_yh - dc_yl;

    // Zero length.
    if (count < 0)
        return;

#ifdef RANGECHECK
    if (static_cast<unsigned>(dc_x) >= SCREENWIDTH || dc_yl < 0
        || dc_yh >= SCREENHEIGHT)
    {
        doom_strcpy(error_buf, "Error: R_DrawColumn: ");
        doom_concat(error_buf, doom_itoa(dc_yl, 10));
        doom_concat(error_buf, " to ");
        doom_concat(error_buf, doom_itoa(dc_yh, 10));
        doom_concat(error_buf, " at ");
        doom_concat(error_buf, doom_itoa(dc_x, 10));
        I_Error(error_buf);
    }
#endif
    // Blocky mode, need to multiply by 2.
    dc_x <<= 1;

    dest = ylookup[dc_yl] + columnofs[dc_x];
    dest2 = ylookup[dc_yl] + columnofs[dc_x + 1];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    do
    {
        // Hack. Does not work corretly.
        *dest2 = *dest = dc_colormap[dc_source[(frac >> FRACBITS) & 127]];
        dest += SCREENWIDTH;
        dest2 += SCREENWIDTH;
        frac += fracstep;

    } while (count--);
}

//
// Spectre/Invisibility.
//

//
// Framebuffer postprocessing.
// Creates a fuzzy image by copying pixels
//  from adjacent ones to left and right.
// Used with an all black colormap, this
//  could create the SHADOW effect,
//  i.e. spectres and invisible players.
//
// The fuzz table and walk position are file-local: only drawFuzzColumn uses them.
int fuzzoffset[FUZZTABLE] = {
    FUZZOFF,  -FUZZOFF, FUZZOFF,  -FUZZOFF, FUZZOFF,  FUZZOFF,  -FUZZOFF, FUZZOFF,
    FUZZOFF,  -FUZZOFF, FUZZOFF,  FUZZOFF,  FUZZOFF,  -FUZZOFF, FUZZOFF,  FUZZOFF,
    FUZZOFF,  -FUZZOFF, -FUZZOFF, -FUZZOFF, -FUZZOFF, FUZZOFF,  -FUZZOFF, -FUZZOFF,
    FUZZOFF,  FUZZOFF,  FUZZOFF,  FUZZOFF,  -FUZZOFF, FUZZOFF,  -FUZZOFF, FUZZOFF,
    FUZZOFF,  -FUZZOFF, -FUZZOFF, FUZZOFF,  FUZZOFF,  -FUZZOFF, -FUZZOFF, -FUZZOFF,
    -FUZZOFF, FUZZOFF,  FUZZOFF,  FUZZOFF,  FUZZOFF,  -FUZZOFF, FUZZOFF,  FUZZOFF,
    -FUZZOFF, FUZZOFF};

static int& fuzzpos = drawTables().fuzzpos;

void drawFuzzColumn()
{
    int count;
    byte* dest;

    // Adjust borders. Low...
    if (!dc_yl)
        dc_yl = 1;

    // .. and high.
    if (dc_yh == viewheight - 1)
        dc_yh = viewheight - 2;

    count = dc_yh - dc_yl;

    // Zero length.
    if (count < 0)
        return;

#ifdef RANGECHECK
    if (static_cast<unsigned>(dc_x) >= SCREENWIDTH || dc_yl < 0
        || dc_yh >= SCREENHEIGHT)
    {
        doom_strcpy(error_buf, "Error: R_DrawFuzzColumn: ");
        doom_concat(error_buf, doom_itoa(dc_yl, 10));
        doom_concat(error_buf, " to ");
        doom_concat(error_buf, doom_itoa(dc_yh, 10));
        doom_concat(error_buf, " at ");
        doom_concat(error_buf, doom_itoa(dc_x, 10));
        I_Error(error_buf);
    }
#endif

    // Does not work with blocky mode.
    dest = ylookup[dc_yl] + columnofs[dc_x];

    // Looks like an attempt at dithering,
    //  using the colormap #6 (of 0-31, a bit
    //  brighter than average). The texture step vanilla computes here is dead:
    //  the fuzz samples the framebuffer, never the source column.
    do
    {
        // Lookup framebuffer, and retrieve
        //  a pixel that is either one column
        //  left or right of the current one.
        // Add index from colormap to index.
        *dest = colormaps[6 * 256 + dest[fuzzoffset[fuzzpos]]];

        // Clamp table lookup index.
        if (++fuzzpos == FUZZTABLE)
            fuzzpos = 0;

        dest += SCREENWIDTH;
    } while (count--);
}

//
// R_DrawTranslatedColumn
// Used to draw player sprites
//  with the green colorramp mapped to others.
// Could be used with different translation
//  tables, e.g. the lighter colored version
//  of the BaronOfHell, the HellKnight, uses
//  identical sprites, kinda brightened up.
//
void drawTranslatedColumn()
{
    int count;
    byte* dest;
    fixed_t frac;
    fixed_t fracstep;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if (static_cast<unsigned>(dc_x) >= SCREENWIDTH || dc_yl < 0
        || dc_yh >= SCREENHEIGHT)
    {
        doom_strcpy(error_buf, "Error: R_DrawColumn: ");
        doom_concat(error_buf, doom_itoa(dc_yl, 10));
        doom_concat(error_buf, " to ");
        doom_concat(error_buf, doom_itoa(dc_yh, 10));
        doom_concat(error_buf, " at ");
        doom_concat(error_buf, doom_itoa(dc_x, 10));
        I_Error(error_buf);
    }
#endif

    // FIXME. As above.
    dest = ylookup[dc_yl] + columnofs[dc_x];

    // Looks familiar.
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // Here we do an additional index re-mapping.
    do
    {
        // Translation tables are used
        //  to map certain colorramps to other ones,
        //  used with PLAY sprites.
        // Thus the "green" ramp of the player 0 sprite
        //  is mapped to gray, red, black/indigo.
        *dest = dc_colormap[dc_translation[dc_source[frac >> FRACBITS]]];
        dest += SCREENWIDTH;

        frac += fracstep;
    } while (count--);
}

//
// R_InitTranslationTables
// Creates the translation tables to map
// the green color ramp to gray, brown, red.
// Assumes a given structure of the PLAYPAL.
// Could be read from a lump instead.
//
void initTranslationTables()
{
    translationtables = static_cast<byte*>(doom_malloc(256 * 3 + 255));
    translationtables =
        (byte*) (((unsigned long long) translationtables + 255) & ~255);

    // translate just the 16 green colors
    for (int i = 0; i < 256; i++)
    {
        if (i >= 0x70 && i <= 0x7f)
        {
            // map green ramp to gray, brown, red
            translationtables[i] = 0x60 + (i & 0xf);
            translationtables[i + 256] = 0x40 + (i & 0xf);
            translationtables[i + 512] = 0x20 + (i & 0xf);
        }
        else
        {
            // Keep all other colors as is.
            translationtables[i] = translationtables[i + 256] =
                translationtables[i + 512] = i;
        }
    }
}

//
// R_DrawSpan
// With DOOM style restrictions on view orientation,
// the floors and ceilings consist of horizontal slices
// or spans with constant z depth.
// However, rotation around the world z axis is possible,
// thus this mapping, while simpler and faster than
// perspective correct texture mapping, has to traverse
// the texture at an angle in all but a few cases.
// In consequence, flats are not stored by column (like walls),
// and the inner loop has to step in texture space u and v.
//
void drawSpan()
{
    fixed_t xfrac;
    fixed_t yfrac;
    byte* dest;
    int count;
    int spot;

#ifdef RANGECHECK
    if (ds_x2 < ds_x1 || ds_x1 < 0 || ds_x2 >= SCREENWIDTH
        || static_cast<unsigned>(ds_y) > SCREENHEIGHT)
    {
        doom_strcpy(error_buf, "Error: R_DrawSpan: ");
        doom_concat(error_buf, doom_itoa(ds_x1, 10));
        doom_concat(error_buf, " to ");
        doom_concat(error_buf, doom_itoa(ds_x2, 10));
        doom_concat(error_buf, " at ");
        doom_concat(error_buf, doom_itoa(ds_y, 10));
        I_Error(error_buf);
    }
#endif

    xfrac = ds_xfrac;
    yfrac = ds_yfrac;

    dest = ylookup[ds_y] + columnofs[ds_x1];

    // We do not check for zero spans here?
    count = ds_x2 - ds_x1;

    do
    {
        // Current texture index in u,v.
        spot = ((yfrac >> (16 - 6)) & (63 * 64)) + ((xfrac >> 16) & 63);

        // Lookup pixel from flat texture tile,
        //  re-index using light/colormap.
        *dest++ = ds_colormap[ds_source[spot]];

        // Next step in u,v.
        xfrac += ds_xstep;
        yfrac += ds_ystep;

    } while (count--);
}

//
// Again..
//
void drawSpanLow()
{
    fixed_t xfrac;
    fixed_t yfrac;
    byte* dest;
    int count;
    int spot;

#ifdef RANGECHECK
    if (ds_x2 < ds_x1 || ds_x1 < 0 || ds_x2 >= SCREENWIDTH
        || static_cast<unsigned>(ds_y) > SCREENHEIGHT)
    {
        doom_strcpy(error_buf, "Error: R_DrawSpan: ");
        doom_concat(error_buf, doom_itoa(ds_x1, 10));
        doom_concat(error_buf, " to ");
        doom_concat(error_buf, doom_itoa(ds_x2, 10));
        doom_concat(error_buf, " at ");
        doom_concat(error_buf, doom_itoa(ds_y, 10));
        I_Error(error_buf);
    }
#endif

    xfrac = ds_xfrac;
    yfrac = ds_yfrac;

    // Blocky mode, need to multiply by 2.
    ds_x1 <<= 1;
    ds_x2 <<= 1;

    dest = ylookup[ds_y] + columnofs[ds_x1];

    count = ds_x2 - ds_x1;
    do
    {
        spot = ((yfrac >> (16 - 6)) & (63 * 64)) + ((xfrac >> 16) & 63);
        // Lowres/blocky mode does it twice,
        //  while scale is adjusted appropriately.
        *dest++ = ds_colormap[ds_source[spot]];
        *dest++ = ds_colormap[ds_source[spot]];

        xfrac += ds_xstep;
        yfrac += ds_ystep;

    } while (count--);
}

//
// R_InitBuffer
// Creats lookup tables that avoid
//  multiplies and other hazzles
//  for getting the framebuffer address
//  of a pixel to draw.
//
void initBuffer(int width, int height)
{
    // Handle resize,
    //  e.g. smaller view windows
    //  with border and/or status bar.
    viewwindowx = (SCREENWIDTH - width) >> 1;

    // Column offset. For windows.
    for (int i = 0; i < width; i++)
        columnofs[i] = viewwindowx + i;

    // Samw with base row offset.
    if (width == SCREENWIDTH)
        viewwindowy = 0;
    else
        viewwindowy = (SCREENHEIGHT - SBARHEIGHT - height) >> 1;

    // Preclaculate all row offsets.
    for (int i = 0; i < height; i++)
        ylookup[i] = screens[0] + (i + viewwindowy) * SCREENWIDTH;
}

//
// R_FillBackScreen
// Fills the back screen with a pattern
//  for variable screen sizes
// Also draws a beveled edge.
//
void fillBackScreen()
{
    byte* src;
    byte* dest;
    int x;
    int y;
    patch_t* patch;

    // DOOM border patch.
    char name1[] = "FLOOR7_2";

    // DOOM II border patch.
    char name2[] = "GRNROCK";

    char* name;

    if (scaledviewwidth == 320)
        return;

    if (gamemode == commercial)
        name = name2;
    else
        name = name1;

    src = static_cast<byte*>(W_CacheLumpName(name, PU_CACHE));
    dest = screens[1];

    for (y = 0; y < SCREENHEIGHT - SBARHEIGHT; y++)
    {
        for (x = 0; x < SCREENWIDTH / 64; x++)
        {
            doom_memcpy(dest, src + ((y & 63) << 6), 64);
            dest += 64;
        }

        if (SCREENWIDTH & 63)
        {
            doom_memcpy(dest, src + ((y & 63) << 6), SCREENWIDTH & 63);
            dest += (SCREENWIDTH & 63);
        }
    }

    patch = static_cast<patch_t*>(W_CacheLumpName("brdr_t", PU_CACHE));

    for (x = 0; x < scaledviewwidth; x += 8)
        V_DrawPatch(viewwindowx + x, viewwindowy - 8, 1, patch);
    patch = static_cast<patch_t*>(W_CacheLumpName("brdr_b", PU_CACHE));

    for (x = 0; x < scaledviewwidth; x += 8)
        V_DrawPatch(viewwindowx + x, viewwindowy + viewheight, 1, patch);
    patch = static_cast<patch_t*>(W_CacheLumpName("brdr_l", PU_CACHE));

    for (y = 0; y < viewheight; y += 8)
        V_DrawPatch(viewwindowx - 8, viewwindowy + y, 1, patch);
    patch = static_cast<patch_t*>(W_CacheLumpName("brdr_r", PU_CACHE));

    for (y = 0; y < viewheight; y += 8)
        V_DrawPatch(viewwindowx + scaledviewwidth, viewwindowy + y, 1, patch);

    // Draw beveled edge.
    V_DrawPatch(viewwindowx - 8,
                viewwindowy - 8,
                1,
                static_cast<patch_t*>(W_CacheLumpName("brdr_tl", PU_CACHE)));

    V_DrawPatch(viewwindowx + scaledviewwidth,
                viewwindowy - 8,
                1,
                static_cast<patch_t*>(W_CacheLumpName("brdr_tr", PU_CACHE)));

    V_DrawPatch(viewwindowx - 8,
                viewwindowy + viewheight,
                1,
                static_cast<patch_t*>(W_CacheLumpName("brdr_bl", PU_CACHE)));

    V_DrawPatch(viewwindowx + scaledviewwidth,
                viewwindowy + viewheight,
                1,
                static_cast<patch_t*>(W_CacheLumpName("brdr_br", PU_CACHE)));
}

//
// Copy a screen buffer.
//
void videoErase(unsigned ofs, int count)
{
    // LFB copy.
    // This might not be a good idea if memcpy
    //  is not optiomal, e.g. byte by byte on
    //  a 32bit CPU, as GNU GCC/Linux libc did
    //  at one point.
    doom_memcpy(screens[0] + ofs, screens[1] + ofs, count);
}

//
// R_DrawViewBorder
// Draws the border around the view
//  for different size windows?
//
void drawViewBorder()
{
    int top;
    int side;
    int ofs;
    int i;

    if (scaledviewwidth == SCREENWIDTH)
        return;

    top = ((SCREENHEIGHT - SBARHEIGHT) - viewheight) / 2;
    side = (SCREENWIDTH - scaledviewwidth) / 2;

    // copy top and one line of left side
    videoErase(0, top * SCREENWIDTH + side);

    // copy one line of right side and bottom
    ofs = (viewheight + top) * SCREENWIDTH - side;
    videoErase(ofs, top * SCREENWIDTH + side);

    // copy sides using wraparound
    ofs = top * SCREENWIDTH + SCREENWIDTH - side;
    side <<= 1;

    for (i = 1; i < viewheight; i++)
    {
        videoErase(ofs, side);
        ofs += SCREENWIDTH;
    }

    // ?
    V_MarkRect(0, 0, SCREENWIDTH, SCREENHEIGHT - SBARHEIGHT);
}

} // namespace Doom
