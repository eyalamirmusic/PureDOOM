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
//        Mission begin melt/wipe screen special effect.
//
//-----------------------------------------------------------------------------


#include "doom_config.h"

#include "z_zone.h"
#include "i_video.h"
#include "v_video.h"
#include "m_random.h"
#include "doomdef.h"
#include "f_wipe.h"


//
// SCREEN WIPE PACKAGE
//

// Raised while a melt is running, and the only safe thing to test: wipe_exitMelt
// frees the column table without clearing the pointer to it.
doom_boolean wipe_melt_running = 0;

byte* wipe_scr_start;
static byte* wipe_scr_end;
static byte* wipe_scr;

// How far down each two-pixel column of the outgoing screen has slid so far.
// Negative means the column has not started moving yet.
int* wipe_melt_offsets;


void wipe_shittyColMajorXform(short* array, int width, int height)
{
    int x;
    int y;
    short* dest;

    dest = (short*)Z_Malloc(width * height * sizeof(short), PU_STATIC, 0);

    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            dest[x * height + y] = array[y * width + x];

    doom_memcpy(array, dest, width * height * 2);

    Z_Free(dest);
}


int wipe_initColorXForm(int width, int height, int ticks)
{
    doom_memcpy(wipe_scr, wipe_scr_start, width * height);
    return 0;
}


int wipe_doColorXForm(int width, int height, int ticks)
{
    doom_boolean changed;
    byte* w;
    byte* e;
    int newval;

    changed = false;
    w = wipe_scr;
    e = wipe_scr_end;

    while (w != wipe_scr + width * height)
    {
        if (*w != *e)
        {
            if (*w > *e)
            {
                newval = *w - ticks;
                if (newval < *e)
                    *w = *e;
                else
                    *w = newval;
                changed = true;
            }
            else if (*w < *e)
            {
                newval = *w + ticks;
                if (newval > *e)
                    *w = *e;
                else
                    *w = newval;
                changed = true;
            }
        }
        w++;
        e++;
    }

    return !changed;
}


int wipe_exitColorXForm(int width, int height, int ticks)
{
    return 0;
}


int wipe_initMelt(int width, int height, int ticks)
{
    int i, r;

    // copy start screen to main screen
    doom_memcpy(wipe_scr, wipe_scr_start, width * height);

    // makes this wipe faster (in theory)
    // to have stuff in column-major format
    wipe_shittyColMajorXform((short*)wipe_scr_start, width / 2, height);
    wipe_shittyColMajorXform((short*)wipe_scr_end, width / 2, height);

    // setup initial column positions
    // (wipe_melt_offsets<0 => not ready to scroll yet)
    wipe_melt_offsets = (int*)Z_Malloc(width * sizeof(int), PU_STATIC, 0);
    wipe_melt_offsets[0] = -(M_Random() % 16);
    for (i = 1; i < width; i++)
    {
        r = (M_Random() % 3) - 1;
        wipe_melt_offsets[i] = wipe_melt_offsets[i - 1] + r;
        if (wipe_melt_offsets[i] > 0) wipe_melt_offsets[i] = 0;
        else if (wipe_melt_offsets[i] == -16) wipe_melt_offsets[i] = -15;
    }

    return 0;
}


int wipe_doMelt(int width, int height, int ticks)
{
    int i;
    int j;
    int dy;
    int idx;

    short* s;
    short* d;
    doom_boolean done = true;

    width /= 2;

    while (ticks--)
    {
        for (i = 0; i < width; i++)
        {
            if (wipe_melt_offsets[i] < 0)
            {
                wipe_melt_offsets[i]++; done = false;
            }
            else if (wipe_melt_offsets[i] < height)
            {
                dy = (wipe_melt_offsets[i] < 16) ? wipe_melt_offsets[i] + 1 : 8;
                if (wipe_melt_offsets[i] + dy >= height) dy = height - wipe_melt_offsets[i];
                s = &((short*)wipe_scr_end)[i * height + wipe_melt_offsets[i]];
                d = &((short*)wipe_scr)[wipe_melt_offsets[i] * width + i];
                idx = 0;
                for (j = dy; j; j--)
                {
                    d[idx] = *(s++);
                    idx += width;
                }
                wipe_melt_offsets[i] += dy;
                s = &((short*)wipe_scr_start)[i * height];
                d = &((short*)wipe_scr)[wipe_melt_offsets[i] * width + i];
                idx = 0;
                for (j = height - wipe_melt_offsets[i]; j; j--)
                {
                    d[idx] = *(s++);
                    idx += width;
                }
                done = false;
            }
        }
    }

    return done;
}


int wipe_exitMelt(int width, int height, int ticks)
{
    Z_Free(wipe_melt_offsets);
    return 0;
}


int wipe_StartScreen(int x, int y, int width, int height)
{
    wipe_scr_start = screens[2];
    I_ReadScreen(wipe_scr_start);
    return 0;
}


int wipe_EndScreen(int x, int y, int width, int height)
{
    wipe_scr_end = screens[3];
    I_ReadScreen(wipe_scr_end);
    V_DrawBlock(x, y, 0, width, height, wipe_scr_start); // restore start scr.
    return 0;
}


int wipe_ScreenWipe(int wipeno, int x, int y, int width, int height, int ticks)
{
    int rc;
    static int (*wipes[])(int, int, int) =
    {
        wipe_initColorXForm, wipe_doColorXForm, wipe_exitColorXForm,
        wipe_initMelt, wipe_doMelt, wipe_exitMelt
    };

    void V_MarkRect(int, int, int, int);

    // initial stuff
    if (!wipe_melt_running)
    {
        wipe_melt_running = 1;
        // wipe_scr = (byte *) Z_Malloc(width*height, PU_STATIC, 0); // DEBUG
        wipe_scr = screens[0];
        (*wipes[wipeno * 3])(width, height, ticks);
    }

    // do a piece of wipe-in
    V_MarkRect(0, 0, width, height);
    rc = (*wipes[wipeno * 3 + 1])(width, height, ticks);
    //  V_DrawBlock(x, y, 0, width, height, wipe_scr); // DEBUG

    // final stuff
    if (rc)
    {
        wipe_melt_running = 0;
        (*wipes[wipeno * 3 + 2])(width, height, ticks);
    }

    return !wipe_melt_running;
}
