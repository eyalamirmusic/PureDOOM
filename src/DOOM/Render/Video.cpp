// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//        Gamma correction LUT stuff.
//        Functions to draw patches (by post) directly to screen.
//        Functions to blit a block to the screen.
//
//-----------------------------------------------------------------------------

// Rewritten out of vanilla v_video into namespace Doom.
//
// Low-level framebuffer drawing: patch/block blitting between the five screens
// and the dirty-box marking. v_video.cpp shims the V_ names. The screens, the
// dirty box, the gamma table and usegamma are the core video state (read across
// the engine and the app), so they stay defined at file scope here, above the
// namespace. The HUD/status bar/menu all draw through these into screens[0], so
// the frame goldens pin the blitters exactly.

#include "../Host/Platform.h"

#include "../Wad/MapFormat.h"
#include "../Game/GameDefs.h"
#include "../Math/Swap.h"

#include "Video.h"
#include "VideoState.h"
#include "../UI/MenuSettings.h"

// Each screen is [SCREENWIDTH*SCREENHEIGHT];
#include "../Host/System.h"
#include "../Math/BBox.h"
byte* screens[5];

// videoState().dirtybox is a Doom::VideoState member (Engine) now; a reference onto it (Step 5).
// It holds screen pixels, not a fixed-point box, so it is grown by addToDirtyBox below rather
// than by Doom::addToBox (Math/BBox.h is still included for the BOXLEFT/BOXBOTTOM/BOXRIGHT/BOXTOP
// index names addToDirtyBox uses).

// Now where did these came from?
EA::Array<EA::Array<byte, 256>, 5> gammatable = {
    {1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,
     17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,
     33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
     49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,
     65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,
     81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,
     97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112,
     113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128,
     128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
     144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
     160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
     176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
     192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
     208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
     224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
     240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255},

    {2,   4,   5,   7,   8,   10,  11,  12,  14,  15,  16,  18,  19,  20,  21,  23,
     24,  25,  26,  27,  29,  30,  31,  32,  33,  34,  36,  37,  38,  39,  40,  41,
     42,  44,  45,  46,  47,  48,  49,  50,  51,  52,  54,  55,  56,  57,  58,  59,
     60,  61,  62,  63,  64,  65,  66,  67,  69,  70,  71,  72,  73,  74,  75,  76,
     77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,
     93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108,
     109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124,
     125, 126, 127, 128, 129, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139,
     140, 141, 142, 143, 144, 145, 146, 147, 148, 148, 149, 150, 151, 152, 153, 154,
     155, 156, 157, 158, 159, 160, 161, 162, 163, 163, 164, 165, 166, 167, 168, 169,
     170, 171, 172, 173, 174, 175, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184,
     185, 186, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 196, 197, 198,
     199, 200, 201, 202, 203, 204, 205, 205, 206, 207, 208, 209, 210, 211, 212, 213,
     214, 214, 215, 216, 217, 218, 219, 220, 221, 222, 222, 223, 224, 225, 226, 227,
     228, 229, 230, 230, 231, 232, 233, 234, 235, 236, 237, 237, 238, 239, 240, 241,
     242, 243, 244, 245, 245, 246, 247, 248, 249, 250, 251, 252, 252, 253, 254, 255},

    {4,   7,   9,   11,  13,  15,  17,  19,  21,  22,  24,  26,  27,  29,  30,  32,
     33,  35,  36,  38,  39,  40,  42,  43,  45,  46,  47,  48,  50,  51,  52,  54,
     55,  56,  57,  59,  60,  61,  62,  63,  65,  66,  67,  68,  69,  70,  72,  73,
     74,  75,  76,  77,  78,  79,  80,  82,  83,  84,  85,  86,  87,  88,  89,  90,
     91,  92,  93,  94,  95,  96,  97,  98,  100, 101, 102, 103, 104, 105, 106, 107,
     108, 109, 110, 111, 112, 113, 114, 114, 115, 116, 117, 118, 119, 120, 121, 122,
     123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 133, 134, 135, 136, 137,
     138, 139, 140, 141, 142, 143, 144, 144, 145, 146, 147, 148, 149, 150, 151, 152,
     153, 153, 154, 155, 156, 157, 158, 159, 160, 160, 161, 162, 163, 164, 165, 166,
     166, 167, 168, 169, 170, 171, 172, 172, 173, 174, 175, 176, 177, 178, 178, 179,
     180, 181, 182, 183, 183, 184, 185, 186, 187, 188, 188, 189, 190, 191, 192, 193,
     193, 194, 195, 196, 197, 197, 198, 199, 200, 201, 201, 202, 203, 204, 205, 206,
     206, 207, 208, 209, 210, 210, 211, 212, 213, 213, 214, 215, 216, 217, 217, 218,
     219, 220, 221, 221, 222, 223, 224, 224, 225, 226, 227, 228, 228, 229, 230, 231,
     231, 232, 233, 234, 235, 235, 236, 237, 238, 238, 239, 240, 241, 241, 242, 243,
     244, 244, 245, 246, 247, 247, 248, 249, 250, 251, 251, 252, 253, 254, 254, 255},

    {8,   12,  16,  19,  22,  24,  27,  29,  31,  34,  36,  38,  40,  41,  43,  45,
     47,  49,  50,  52,  53,  55,  57,  58,  60,  61,  63,  64,  65,  67,  68,  70,
     71,  72,  74,  75,  76,  77,  79,  80,  81,  82,  84,  85,  86,  87,  88,  90,
     91,  92,  93,  94,  95,  96,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107,
     108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123,
     124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 135, 136, 137, 138,
     139, 140, 141, 142, 143, 143, 144, 145, 146, 147, 148, 149, 150, 150, 151, 152,
     153, 154, 155, 155, 156, 157, 158, 159, 160, 160, 161, 162, 163, 164, 165, 165,
     166, 167, 168, 169, 169, 170, 171, 172, 173, 173, 174, 175, 176, 176, 177, 178,
     179, 180, 180, 181, 182, 183, 183, 184, 185, 186, 186, 187, 188, 189, 189, 190,
     191, 192, 192, 193, 194, 195, 195, 196, 197, 197, 198, 199, 200, 200, 201, 202,
     202, 203, 204, 205, 205, 206, 207, 207, 208, 209, 210, 210, 211, 212, 212, 213,
     214, 214, 215, 216, 216, 217, 218, 219, 219, 220, 221, 221, 222, 223, 223, 224,
     225, 225, 226, 227, 227, 228, 229, 229, 230, 231, 231, 232, 233, 233, 234, 235,
     235, 236, 237, 237, 238, 238, 239, 240, 240, 241, 242, 242, 243, 244, 244, 245,
     246, 246, 247, 247, 248, 249, 249, 250, 251, 251, 252, 253, 253, 254, 254, 255},

    {16,  23,  28,  32,  36,  39,  42,  45,  48,  50,  53,  55,  57,  60,  62,
     64,  66,  68,  69,  71,  73,  75,  76,  78,  80,  81,  83,  84,  86,  87,
     89,  90,  92,  93,  94,  96,  97,  98,  100, 101, 102, 103, 105, 106, 107,
     108, 109, 110, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123,
     124, 125, 126, 128, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138,
     139, 140, 141, 142, 143, 143, 144, 145, 146, 147, 148, 149, 150, 150, 151,
     152, 153, 154, 155, 155, 156, 157, 158, 159, 159, 160, 161, 162, 163, 163,
     164, 165, 166, 166, 167, 168, 169, 169, 170, 171, 172, 172, 173, 174, 175,
     175, 176, 177, 177, 178, 179, 180, 180, 181, 182, 182, 183, 184, 184, 185,
     186, 187, 187, 188, 189, 189, 190, 191, 191, 192, 193, 193, 194, 195, 195,
     196, 196, 197, 198, 198, 199, 200, 200, 201, 202, 202, 203, 203, 204, 205,
     205, 206, 207, 207, 208, 208, 209, 210, 210, 211, 211, 212, 213, 213, 214,
     214, 215, 216, 216, 217, 217, 218, 219, 219, 220, 220, 221, 221, 222, 223,
     223, 224, 224, 225, 225, 226, 227, 227, 228, 228, 229, 229, 230, 230, 231,
     232, 232, 233, 233, 234, 234, 235, 235, 236, 236, 237, 237, 238, 239, 239,
     240, 240, 241, 241, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 247,
     247, 248, 248, 249, 249, 250, 250, 251, 251, 252, 252, 253, 254, 254, 255,
     255}};

// usegamma is config-backed and owned by the Engine's MenuSettings cluster now
// (UI/MenuSettings.h); this is a reference onto that member. Config.cpp binds its
// defaults[] entry at runtime rather than capturing the address at static-init.

namespace Doom
{

namespace
{
// Grows dirtybox by one point. The same else-if update Doom::BBox::add (Math/BBox.h)
// uses, but on plain screen-pixel ints - dirtybox is not a real fixed-point box.
void addToDirtyBox(int x, int y)
{
    auto& box = videoState().dirtybox;

    if (x < box[BOXLEFT])
        box[BOXLEFT] = x;
    else if (x > box[BOXRIGHT])
        box[BOXRIGHT] = x;

    if (y < box[BOXBOTTOM])
        box[BOXBOTTOM] = y;
    else if (y > box[BOXTOP])
        box[BOXTOP] = y;
}
} // namespace

//
// markRect
//
void markRect(int x, int y, int width, int height)
{
    addToDirtyBox(x, y);
    addToDirtyBox(x + width - 1, y + height - 1);
}

//
// copyRect
//
void copyRect(int srcx,
              int srcy,
              int srcscrn,
              int width,
              int height,
              int destx,
              int desty,
              int destscrn)
{
    byte* src;
    byte* dest;

#ifdef RANGECHECK
    if (srcx < 0 || srcx + width > SCREENWIDTH || srcy < 0
        || srcy + height > SCREENHEIGHT || destx < 0 || destx + width > SCREENWIDTH
        || desty < 0 || desty + height > SCREENHEIGHT
        || static_cast<unsigned>(srcscrn) > 4 || static_cast<unsigned>(destscrn) > 4)
    {
        fatalError("Error: Bad copyRect");
    }
#endif
    markRect(destx, desty, width, height);

    src = screens[srcscrn] + SCREENWIDTH * srcy + srcx;
    dest = screens[destscrn] + SCREENWIDTH * desty + destx;

    for (; height > 0; height--)
    {
        doom_memcpy(dest, src, width);
        src += SCREENWIDTH;
        dest += SCREENWIDTH;
    }
}

//
// drawPatch
// Masks a column based masked pic to the screen.
//
void drawPatch(int x, int y, int scrn, Patch* patch)
{
    int count;
    int col;
    Column* column;
    byte* desttop;
    byte* dest;
    byte* source;
    int w;

    y -= littleEndian(patch->topoffset);
    x -= littleEndian(patch->leftoffset);
#ifdef RANGECHECK
    if (x < 0 || x + littleEndian(patch->width) > SCREENWIDTH || y < 0
        || y + littleEndian(patch->height) > SCREENHEIGHT
        || static_cast<unsigned>(scrn) > 4)
    {
        //doom_print("Patch at %d,%d exceeds LFB\n", x, y);
        print("Patch at ", x, ",", y, " exceeds LFB\n");
        // No fatalError abort - what is up with TNT.WAD?
        print("drawPatch: bad patch (ignored)\n");
        return;
    }
#endif

    if (!scrn)
        markRect(x, y, littleEndian(patch->width), littleEndian(patch->height));

    col = 0;
    desttop = screens[scrn] + y * SCREENWIDTH + x;

    w = littleEndian(patch->width);

    for (; col < w; x++, col++, desttop++)
    {
        column = reinterpret_cast<Column*>(reinterpret_cast<byte*>(patch)
                                           + littleEndian(patch->columnofs[col]));

        // step through the posts in a column
        while (column->topdelta != 0xff)
        {
            source = reinterpret_cast<byte*>(column) + 3;
            dest = desttop + column->topdelta * SCREENWIDTH;
            count = column->length;

            while (count--)
            {
                *dest = *source++;
                dest += SCREENWIDTH;
            }
            column = reinterpret_cast<Column*>(reinterpret_cast<byte*>(column)
                                               + column->length + 4);
        }
    }
}

//
// drawPatchFlipped
// Masks a column based masked pic to the screen.
// Flips horizontally, e.g. to mirror face.
//
void drawPatchFlipped(int x, int y, int scrn, Patch* patch)
{
    int count;
    int col;
    Column* column;
    byte* desttop;
    byte* dest;
    byte* source;
    int w;

    y -= littleEndian(patch->topoffset);
    x -= littleEndian(patch->leftoffset);
#ifdef RANGECHECK
    if (x < 0 || x + littleEndian(patch->width) > SCREENWIDTH || y < 0
        || y + littleEndian(patch->height) > SCREENHEIGHT
        || static_cast<unsigned>(scrn) > 4)
    {
        //doom_print("Patch origin %d,%d exceeds LFB\n", x, y);
        print("Patch origin ", x, ",", y, " exceeds LFB\n");
        fatalError("Error: Bad drawPatch in drawPatchFlipped");
    }
#endif

    if (!scrn)
        markRect(x, y, littleEndian(patch->width), littleEndian(patch->height));

    col = 0;
    desttop = screens[scrn] + y * SCREENWIDTH + x;

    w = littleEndian(patch->width);

    for (; col < w; x++, col++, desttop++)
    {
        column =
            reinterpret_cast<Column*>(reinterpret_cast<byte*>(patch)
                                      + littleEndian(patch->columnofs[w - 1 - col]));

        // step through the posts in a column
        while (column->topdelta != 0xff)
        {
            source = reinterpret_cast<byte*>(column) + 3;
            dest = desttop + column->topdelta * SCREENWIDTH;
            count = column->length;

            while (count--)
            {
                *dest = *source++;
                dest += SCREENWIDTH;
            }
            column = reinterpret_cast<Column*>(reinterpret_cast<byte*>(column)
                                               + column->length + 4);
        }
    }
}

// src_x/src_w are NOT byte-swapped, unlike the patch's own header fields. They are
// this port's own addition (Menu.cpp's one call site passes a MenuCustomTextSeg's
// members, built in C++), so they never came off disk and there is nothing to
// swap. The SHORT() this had inherited from the surrounding patch-width reads was
// identity on a little-endian host and would have swapped a screen-space width on
// a big-endian one.
void drawPatchRectDirect(int x, int y, int scrn, Patch* patch, int src_x, int src_w)
{
    int count;
    int col;
    Column* column;
    byte* desttop;
    byte* dest;
    byte* source;
    int w;

    y -= littleEndian(patch->topoffset);
    x -= littleEndian(patch->leftoffset);
#ifdef RANGECHECK
    if (x < 0 || x + src_w > SCREENWIDTH || y < 0
        || y + littleEndian(patch->height) > SCREENHEIGHT
        || static_cast<unsigned>(scrn) > 4)
    {
        //doom_print("Patch at %d,%d exceeds LFB\n", x, y);
        print("Patch at ", x, ",", y, " exceeds LFB\n");
        // No fatalError abort - what is up with TNT.WAD?
        print("drawPatch: bad patch (ignored)\n");
        return;
    }
#endif

    if (!scrn)
        markRect(x, y, src_w, littleEndian(patch->height));

    col = 0;
    desttop = screens[scrn] + y * SCREENWIDTH + x;

    w = src_w;

    for (; col < w; x++, col++, desttop++)
    {
        column =
            reinterpret_cast<Column*>(reinterpret_cast<byte*>(patch)
                                      + littleEndian(patch->columnofs[col + src_x]));

        // step through the posts in a column
        while (column->topdelta != 0xff)
        {
            source = reinterpret_cast<byte*>(column) + 3;
            dest = desttop + column->topdelta * SCREENWIDTH;
            count = column->length;

            while (count--)
            {
                *dest = *source++;
                dest += SCREENWIDTH;
            }
            column = reinterpret_cast<Column*>(reinterpret_cast<byte*>(column)
                                               + column->length + 4);
        }
    }
}

//
// drawPatchDirect
// Draws directly to the screen on the pc.
//
void drawPatchDirect(int x, int y, int scrn, Patch* patch)
{
    drawPatch(x, y, scrn, patch);
}

//
// drawBlock
// Draw a linear block of pixels into the view buffer.
//
void drawBlock(int x, int y, int scrn, int width, int height, byte* src)
{
    byte* dest;

#ifdef RANGECHECK
    if (x < 0 || x + width > SCREENWIDTH || y < 0 || y + height > SCREENHEIGHT
        || static_cast<unsigned>(scrn) > 4)
    {
        fatalError("Error: Bad drawBlock");
    }
#endif

    markRect(x, y, width, height);

    dest = screens[scrn] + y * SCREENWIDTH + x;

    while (height--)
    {
        doom_memcpy(dest, src, width);
        src += width;
        dest += SCREENWIDTH;
    }
}

//
// getBlock
// Gets a linear block of pixels from the view buffer.
//
void getBlock(int x, int y, int scrn, int width, int height, byte* dest)
{
    byte* src;

#ifdef RANGECHECK
    if (x < 0 || x + width > SCREENWIDTH || y < 0 || y + height > SCREENHEIGHT
        || static_cast<unsigned>(scrn) > 4)
    {
        fatalError("Error: Bad drawBlock");
    }
#endif

    src = screens[scrn] + y * SCREENWIDTH + x;

    while (height--)
    {
        doom_memcpy(dest, src, width);
        src += SCREENWIDTH;
        dest += width;
    }
}

//
// initVideo
//
void initVideo()
{
    // stick these in low dos memory on PCs

    // RAII now (Step 9): the base block is VideoState's workspace vector, zero-filled
    // by assign() below and sliced into the screens[] view. screens[0]'s slice is
    // overwritten by initGraphics, exactly as vanilla left it.
    auto& workspace = videoState().workspace;
    workspace.assign(SCREENWIDTH * SCREENHEIGHT * 4, byte(0));
    byte* base = workspace.data();

    for (int i = 0; i < 4; i++)
        screens[i] = base + i * SCREENWIDTH * SCREENHEIGHT;
}

} // namespace Doom
