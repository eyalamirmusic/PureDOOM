// Rewritten out of vanilla r_draw into namespace Doom.
//
// The actual column and span blitting: the wall/sprite column drawers, the
// spectre fuzz and translated-colour variants, the flat span drawers, and the
// view-buffer / border setup they all draw through. r_draw.cpp shims the R_ names
// (r_main stores those shim addresses in colfunc/spanfunc/...) and owns the
// drawer input state (dc_*, ds_*) the other renderer files fill in; the frame
// address tables and the fuzz walk are file-local here. Golden-neutral.

#include "../Host/Platform.h"

#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h" // State.
#include "../Wad/WadFile.h"

#include "Draw.h"
#include "DrawState.h"
#include "DrawTables.h"
#include "ViewProjection.h"
#include "ViewWindow.h"

#include "Video.h"
#include <ea_data_structures/Structures/Array.h>

// ?
#include "../Host/System.h"
#include "../Game/GameVersion.h"
#include "../Render/GraphicsData.h"

namespace Doom
{

// status bar height at bottom of screen
constexpr int SBARHEIGHT = 32;

constexpr int FUZZTABLE = 50;
constexpr int FUZZOFF = SCREENWIDTH;

//
// All drawing to the view buffer is accomplished in this file. The other refresh
// files only know about coordinates, not the architecture of the frame buffer.
// Conveniently, the frame buffer is a linear one, and we need only the base
// address, and the total size == width*height*depth/8.
//
// These frame-address lookup tables (and fuzzpos below) now live on the Engine (Render/DrawTables.h,
// moved by the file-scope-statics sweep - REFACTOR.md, Step 5). Every drawer below hoists
// drawTables() once and reaches them through it, rather than through file-scope reference aliases
// (REFACTOR.md, Step 9 strand (a)); only the drawers use them, through Doom::initBuffer.

//
// A column is a vertical slice/span from a wall texture that, given the DOOM
// style restrictions on the view orientation, will always have constant z depth.
// Thus a special case loop for very fast rendering can be used. It has also been
// used with Wolfenstein 3D.
//
void drawColumn()
{
    auto& draw = drawState();
    auto& tables = drawTables();

    int count;
    byte* dest;
    fixed_t frac;
    fixed_t fracstep;

    count = draw.dc_yh - draw.dc_yl;

    // Zero length, column does not exceed a pixel.
    if (count < 0)
        return;

#ifdef RANGECHECK
    if (static_cast<unsigned>(draw.dc_x) >= SCREENWIDTH || draw.dc_yl < 0
        || draw.dc_yh >= SCREENHEIGHT)
    {
        fatalError("Error: Doom::drawColumn: ",
                   draw.dc_yl,
                   " to ",
                   draw.dc_yh,
                   " at ",
                   draw.dc_x);
    }
#endif

    // Framebuffer destination address.
    // Use ylookup LUT to avoid multiply with ScreenWidth.
    // Use columnofs LUT for subwindows?
    dest = tables.ylookup[draw.dc_yl] + tables.columnofs[draw.dc_x];

    // Determine scaling,
    //  which is the only mapping to be done.
    fracstep = draw.dc_iscale;
    frac = draw.dc_texturemid + (draw.dc_yl - viewProjection().centery) * fracstep;

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.
    do
    {
        // Re-map color indices from wall texture column
        //  using a lighting/special effects LUT.
        *dest = draw.dc_colormap[draw.dc_source[frac.toInt() & 127]];

        dest += SCREENWIDTH;
        frac += fracstep;

    } while (count--);
}

void drawColumnLow()
{
    auto& draw = drawState();
    auto& tables = drawTables();

    int count;
    byte* dest;
    byte* dest2;
    fixed_t frac;
    fixed_t fracstep;

    count = draw.dc_yh - draw.dc_yl;

    // Zero length.
    if (count < 0)
        return;

#ifdef RANGECHECK
    if (static_cast<unsigned>(draw.dc_x) >= SCREENWIDTH || draw.dc_yl < 0
        || draw.dc_yh >= SCREENHEIGHT)
    {
        fatalError("Error: Doom::drawColumn: ",
                   draw.dc_yl,
                   " to ",
                   draw.dc_yh,
                   " at ",
                   draw.dc_x);
    }
#endif
    // Blocky mode, need to multiply by 2.
    draw.dc_x <<= 1;

    dest = tables.ylookup[draw.dc_yl] + tables.columnofs[draw.dc_x];
    dest2 = tables.ylookup[draw.dc_yl] + tables.columnofs[draw.dc_x + 1];

    fracstep = draw.dc_iscale;
    frac = draw.dc_texturemid + (draw.dc_yl - viewProjection().centery) * fracstep;

    do
    {
        // Hack. Does not work corretly.
        *dest2 = *dest = draw.dc_colormap[draw.dc_source[frac.toInt() & 127]];
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
EA::Array<int, FUZZTABLE> fuzzoffset = {
    FUZZOFF,  -FUZZOFF, FUZZOFF,  -FUZZOFF, FUZZOFF,  FUZZOFF,  -FUZZOFF, FUZZOFF,
    FUZZOFF,  -FUZZOFF, FUZZOFF,  FUZZOFF,  FUZZOFF,  -FUZZOFF, FUZZOFF,  FUZZOFF,
    FUZZOFF,  -FUZZOFF, -FUZZOFF, -FUZZOFF, -FUZZOFF, FUZZOFF,  -FUZZOFF, -FUZZOFF,
    FUZZOFF,  FUZZOFF,  FUZZOFF,  FUZZOFF,  -FUZZOFF, FUZZOFF,  -FUZZOFF, FUZZOFF,
    FUZZOFF,  -FUZZOFF, -FUZZOFF, FUZZOFF,  FUZZOFF,  -FUZZOFF, -FUZZOFF, -FUZZOFF,
    -FUZZOFF, FUZZOFF,  FUZZOFF,  FUZZOFF,  FUZZOFF,  -FUZZOFF, FUZZOFF,  FUZZOFF,
    -FUZZOFF, FUZZOFF};

void drawFuzzColumn()
{
    auto& draw = drawState();
    auto& view = viewWindow();
    auto& tables = drawTables();

    int count;
    byte* dest;

    // Adjust borders. Low...
    if (!draw.dc_yl)
        draw.dc_yl = 1;

    // .. and high.
    if (draw.dc_yh == view.viewheight - 1)
        draw.dc_yh = view.viewheight - 2;

    count = draw.dc_yh - draw.dc_yl;

    // Zero length.
    if (count < 0)
        return;

#ifdef RANGECHECK
    if (static_cast<unsigned>(draw.dc_x) >= SCREENWIDTH || draw.dc_yl < 0
        || draw.dc_yh >= SCREENHEIGHT)
    {
        fatalError("Error: Doom::drawFuzzColumn: ",
                   draw.dc_yl,
                   " to ",
                   draw.dc_yh,
                   " at ",
                   draw.dc_x);
    }
#endif

    // Does not work with blocky mode.
    dest = tables.ylookup[draw.dc_yl] + tables.columnofs[draw.dc_x];

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
        *dest = colormaps[6 * 256 + dest[fuzzoffset[tables.fuzzpos]]];

        // Clamp table lookup index.
        if (++tables.fuzzpos == FUZZTABLE)
            tables.fuzzpos = 0;

        dest += SCREENWIDTH;
    } while (count--);
}

//
// Doom::drawTranslatedColumn
// Used to draw player sprites
//  with the green colorramp mapped to others.
// Could be used with different translation
//  tables, e.g. the lighter colored version
//  of the BaronOfHell, the HellKnight, uses
//  identical sprites, kinda brightened up.
//
void drawTranslatedColumn()
{
    auto& draw = drawState();
    auto& tables = drawTables();

    int count;
    byte* dest;
    fixed_t frac;
    fixed_t fracstep;

    count = draw.dc_yh - draw.dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if (static_cast<unsigned>(draw.dc_x) >= SCREENWIDTH || draw.dc_yl < 0
        || draw.dc_yh >= SCREENHEIGHT)
    {
        fatalError("Error: Doom::drawColumn: ",
                   draw.dc_yl,
                   " to ",
                   draw.dc_yh,
                   " at ",
                   draw.dc_x);
    }
#endif

    // FIXME. As above.
    dest = tables.ylookup[draw.dc_yl] + tables.columnofs[draw.dc_x];

    // Looks familiar.
    fracstep = draw.dc_iscale;
    frac = draw.dc_texturemid + (draw.dc_yl - viewProjection().centery) * fracstep;

    // Here we do an additional index re-mapping.
    do
    {
        // Translation tables are used
        //  to map certain colorramps to other ones,
        //  used with PLAY sprites.
        // Thus the "green" ramp of the player 0 sprite
        //  is mapped to gray, red, black/indigo.
        *dest = draw.dc_colormap[draw.dc_translation[draw.dc_source[frac.toInt()]]];
        dest += SCREENWIDTH;

        frac += fracstep;
    } while (count--);
}

//
// Doom::initTranslationTables
// Creates the translation tables to map
// the green color ramp to gray, brown, red.
// Assumes a given structure of the PLAYPAL.
// Could be read from a lump instead.
//
void initTranslationTables()
{
    // DrawState owns the backing buffer now (RAII, Step 9); translationtables is the
    // 256-byte-aligned view into it, as the original doom_malloc + align was.
    auto& ds = drawState();
    ds.translationTableStorage.resize(256 * 3 + 255);
    translationtables = reinterpret_cast<byte*>(
        (reinterpret_cast<unsigned long long>(ds.translationTableStorage.data())
         + 255)
        & ~255ULL);

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
// Doom::drawSpan
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
    auto& draw = drawState();
    auto& tables = drawTables();

    fixed_t xfrac;
    fixed_t yfrac;
    byte* dest;
    int count;
    int spot;

#ifdef RANGECHECK
    if (draw.ds_x2 < draw.ds_x1 || draw.ds_x1 < 0 || draw.ds_x2 >= SCREENWIDTH
        || static_cast<unsigned>(draw.ds_y) > SCREENHEIGHT)
    {
        fatalError("Error: Doom::drawSpan: ",
                   draw.ds_x1,
                   " to ",
                   draw.ds_x2,
                   " at ",
                   draw.ds_y);
    }
#endif

    xfrac = draw.ds_xfrac;
    yfrac = draw.ds_yfrac;

    dest = tables.ylookup[draw.ds_y] + tables.columnofs[draw.ds_x1];

    // We do not check for zero spans here?
    count = draw.ds_x2 - draw.ds_x1;

    do
    {
        // Current texture index in u,v.
        spot = ((yfrac.raw >> (16 - 6)) & (63 * 64)) + ((xfrac.raw >> 16) & 63);

        // Lookup pixel from flat texture tile,
        //  re-index using light/colormap.
        *dest++ = draw.ds_colormap[draw.ds_source[spot]];

        // Next step in u,v.
        xfrac += draw.ds_xstep;
        yfrac += draw.ds_ystep;

    } while (count--);
}

//
// Again..
//
void drawSpanLow()
{
    auto& draw = drawState();
    auto& tables = drawTables();

    fixed_t xfrac;
    fixed_t yfrac;
    byte* dest;
    int count;
    int spot;

#ifdef RANGECHECK
    if (draw.ds_x2 < draw.ds_x1 || draw.ds_x1 < 0 || draw.ds_x2 >= SCREENWIDTH
        || static_cast<unsigned>(draw.ds_y) > SCREENHEIGHT)
    {
        fatalError("Error: Doom::drawSpan: ",
                   draw.ds_x1,
                   " to ",
                   draw.ds_x2,
                   " at ",
                   draw.ds_y);
    }
#endif

    xfrac = draw.ds_xfrac;
    yfrac = draw.ds_yfrac;

    // Blocky mode, need to multiply by 2.
    draw.ds_x1 <<= 1;
    draw.ds_x2 <<= 1;

    dest = tables.ylookup[draw.ds_y] + tables.columnofs[draw.ds_x1];

    count = draw.ds_x2 - draw.ds_x1;
    do
    {
        spot = ((yfrac.raw >> (16 - 6)) & (63 * 64)) + ((xfrac.raw >> 16) & 63);
        // Lowres/blocky mode does it twice,
        //  while scale is adjusted appropriately.
        *dest++ = draw.ds_colormap[draw.ds_source[spot]];
        *dest++ = draw.ds_colormap[draw.ds_source[spot]];

        xfrac += draw.ds_xstep;
        yfrac += draw.ds_ystep;

    } while (count--);
}

//
// Doom::initBuffer
// Creats lookup tables that avoid
//  multiplies and other hazzles
//  for getting the framebuffer address
//  of a pixel to draw.
//
void initBuffer(int width, int height)
{
    auto& view = viewWindow();
    auto& tables = drawTables();

    // Handle resize,
    //  e.g. smaller view windows
    //  with border and/or status bar.
    view.viewwindowx = (SCREENWIDTH - width) >> 1;

    // Column offset. For windows.
    for (int i = 0; i < width; i++)
        tables.columnofs[i] = view.viewwindowx + i;

    // Samw with base row offset.
    if (width == SCREENWIDTH)
        view.viewwindowy = 0;
    else
        view.viewwindowy = (SCREENHEIGHT - SBARHEIGHT - height) >> 1;

    // Preclaculate all row offsets.
    for (int i = 0; i < height; i++)
        tables.ylookup[i] = screens[0] + (i + view.viewwindowy) * SCREENWIDTH;
}

//
// Doom::fillBackScreen
// Fills the back screen with a pattern
//  for variable screen sizes
// Also draws a beveled edge.
//
void fillBackScreen()
{
    auto& view = viewWindow();

    byte* src;
    byte* dest;
    int x;
    int y;
    Patch* patch;

    // DOOM border patch.
    char name1[] = "FLOOR7_2";

    // DOOM II border patch.
    char name2[] = "GRNROCK";

    char* name;

    if (view.scaledviewwidth == 320)
        return;

    if (gameVersion().gamemode == commercial)
        name = name2;
    else
        name = name1;

    src = static_cast<byte*>(Doom::cacheLumpName(name));
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

    patch = static_cast<Patch*>(Doom::cacheLumpName("brdr_t"));

    for (x = 0; x < view.scaledviewwidth; x += 8)
        Doom::drawPatch(view.viewwindowx + x, view.viewwindowy - 8, 1, patch);
    patch = static_cast<Patch*>(Doom::cacheLumpName("brdr_b"));

    for (x = 0; x < view.scaledviewwidth; x += 8)
        Doom::drawPatch(
            view.viewwindowx + x, view.viewwindowy + view.viewheight, 1, patch);
    patch = static_cast<Patch*>(Doom::cacheLumpName("brdr_l"));

    for (y = 0; y < view.viewheight; y += 8)
        Doom::drawPatch(view.viewwindowx - 8, view.viewwindowy + y, 1, patch);
    patch = static_cast<Patch*>(Doom::cacheLumpName("brdr_r"));

    for (y = 0; y < view.viewheight; y += 8)
        Doom::drawPatch(
            view.viewwindowx + view.scaledviewwidth, view.viewwindowy + y, 1, patch);

    // Draw beveled edge.
    Doom::drawPatch(view.viewwindowx - 8,
                    view.viewwindowy - 8,
                    1,
                    static_cast<Patch*>(Doom::cacheLumpName("brdr_tl")));

    Doom::drawPatch(view.viewwindowx + view.scaledviewwidth,
                    view.viewwindowy - 8,
                    1,
                    static_cast<Patch*>(Doom::cacheLumpName("brdr_tr")));

    Doom::drawPatch(view.viewwindowx - 8,
                    view.viewwindowy + view.viewheight,
                    1,
                    static_cast<Patch*>(Doom::cacheLumpName("brdr_bl")));

    Doom::drawPatch(view.viewwindowx + view.scaledviewwidth,
                    view.viewwindowy + view.viewheight,
                    1,
                    static_cast<Patch*>(Doom::cacheLumpName("brdr_br")));
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
// Doom::drawViewBorder
// Draws the border around the view
//  for different size windows?
//
void drawViewBorder()
{
    auto& view = viewWindow();

    int top;
    int side;
    int ofs;
    int i;

    if (view.scaledviewwidth == SCREENWIDTH)
        return;

    top = ((SCREENHEIGHT - SBARHEIGHT) - view.viewheight) / 2;
    side = (SCREENWIDTH - view.scaledviewwidth) / 2;

    // copy top and one line of left side
    videoErase(0, top * SCREENWIDTH + side);

    // copy one line of right side and bottom
    ofs = (view.viewheight + top) * SCREENWIDTH - side;
    videoErase(ofs, top * SCREENWIDTH + side);

    // copy sides using wraparound
    ofs = top * SCREENWIDTH + SCREENWIDTH - side;
    side <<= 1;

    for (i = 1; i < view.viewheight; i++)
    {
        videoErase(ofs, side);
        ofs += SCREENWIDTH;
    }

    // ?
    Doom::markRect(0, 0, SCREENWIDTH, SCREENHEIGHT - SBARHEIGHT);
}

} // namespace Doom

// ---------------------------------------------------------------------------
// Global-scope data that was r_draw.cpp. It stays at :: scope because these are the
// vanilla names other translation units (and the eacp port) still link against.
// ---------------------------------------------------------------------------
//
// The view window geometry (set by r_main, read across the renderer and app). The
// storage is a Doom::ViewWindow owned by the Engine now; these vanilla names are
// references onto it.
//

//
// The column/span drawer inputs are a Doom::DrawState owned by the Engine now; these are
// references onto its members (REFACTOR.md, Step 5).
//

//
// R_DrawColumn input: the caller (r_segs/r_plane/r_things) fills these in, the
// column drawers in Render/Draw.cpp read them.
//

// first pixel in a column (possibly virtual)

// Translation tables for player-sprite recolouring (read by r_things).
// A 256-byte-aligned view into DrawState's owned translationTableStorage;
// R_InitTranslationTables points it at the aligned offset (Step 9).
byte* translationtables = nullptr;

//
// R_DrawSpan input: r_plane fills these in, the span drawers read them.
//

// start of a 64*64 tile image
