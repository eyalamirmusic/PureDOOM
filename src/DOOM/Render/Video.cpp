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

// videoState().dirtybox is a Doom::VideoState member (Engine) now; a reference onto it (Step 5).
// It holds screen pixels, not a fixed-point box, so it is grown by addToDirtyBox below rather
// than by Doom::addToBox (Math/BBox.h is still included for the boxLeft/boxBottom/boxRight/boxTop
// index names addToDirtyBox uses).

// The gamma-correction table (v_video's gammatable) was defined here and read
// nowhere, in either era: usegamma selects a row of it but nothing applies one -
// the eacp host does its own output and never gamma-corrects. Deleted rather than
// carried, the way the macro sweep retired the other dead-in-both-eras data.

// usegamma is config-backed and owned by the Engine's MenuSettings cluster now
// (UI/MenuSettings.h); this is a reference onto that member. Config.cpp binds its
// defaults[] entry at runtime rather than capturing the address at static-init.

namespace Doom
{

namespace
{
// Grows dirtybox by one point. The same else-if update BBox::add (Math/BBox.h)
// uses, but on plain screen-pixel ints - dirtybox is not a real fixed-point box.
void addToDirtyBox(int x, int y)
{
    auto& box = videoState().dirtybox;

    if (x < box[boxLeft])
        box[boxLeft] = x;
    else if (x > box[boxRight])
        box[boxRight] = x;

    if (y < box[boxBottom])
        box[boxBottom] = y;
    else if (y > box[boxTop])
        box[boxTop] = y;
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

    src = videoState().screens[srcscrn] + SCREENWIDTH * srcy + srcx;
    dest = videoState().screens[destscrn] + SCREENWIDTH * desty + destx;

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
    desttop = videoState().screens[scrn] + y * SCREENWIDTH + x;

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
    desttop = videoState().screens[scrn] + y * SCREENWIDTH + x;

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
    desttop = videoState().screens[scrn] + y * SCREENWIDTH + x;

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

    dest = videoState().screens[scrn] + y * SCREENWIDTH + x;

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

    src = videoState().screens[scrn] + y * SCREENWIDTH + x;

    while (height--)
    {
        doom_memcpy(dest, src, width);
        src += SCREENWIDTH;
        dest += width;
    }
}

//
// VideoState
//
// What initVideo used to do at boot, now owned by construction (see VideoState.h):
// the base block is the workspace vector, zero-filled by assign() and sliced into the
// screens[] view. screens[0]'s slice is overwritten by initGraphics, exactly as
// vanilla left it.
VideoState::VideoState()
{
    workspace.assign(SCREENWIDTH * SCREENHEIGHT * 4, byte(0));
    byte* base = workspace.data();

    for (int i = 0; i < 4; i++)
        screens[i] = base + i * SCREENWIDTH * SCREENHEIGHT;
}

} // namespace Doom
