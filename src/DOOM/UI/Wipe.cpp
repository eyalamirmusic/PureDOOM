// Rewritten out of vanilla f_wipe into namespace Doom.
//
// The mission-begin melt/wipe. f_wipe.cpp shims the three wipe_ entry points and
// owns the state a GPU compositor reads (wipe_melt_running, wipe_scr_start, the
// column offsets); the scratch screens and the melt/colour-transform helpers are
// file-local here. The melt walks M_Random (not P_Random), so it never desyncs
// the simulation. Golden-neutral.

#include "../doom_config.h"

#include "../doomdef.h"
#include "../i_video.h"
#include "../m_random.h"
#include "../v_video.h"
#include "../z_zone.h"

#include "../f_wipe.h" // the shim's globals (wipe_scr_start / offsets / running)
#include "Wipe.h"

namespace Doom
{

// File-local scratch: the incoming frame and the working frame.
byte* wipe_scr_end;
byte* wipe_scr;

void colMajorXform(short* array, int width, int height)
{
    int x;
    int y;
    short* dest;

    dest = (short*) Z_Malloc(width * height * sizeof(short), PU_STATIC, 0);

    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            dest[x * height + y] = array[y * width + x];

    doom_memcpy(array, dest, width * height * 2);

    Z_Free(dest);
}

int initColorXForm(int width, int height, int ticks)
{
    (void) ticks;
    doom_memcpy(wipe_scr, wipe_scr_start, width * height);
    return 0;
}

int doColorXForm(int width, int height, int ticks)
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

int exitColorXForm(int width, int height, int ticks)
{
    (void) width;
    (void) height;
    (void) ticks;
    return 0;
}

int initMelt(int width, int height, int ticks)
{
    int i, r;
    (void) ticks;

    // copy start screen to main screen
    doom_memcpy(wipe_scr, wipe_scr_start, width * height);

    // makes this wipe faster (in theory)
    // to have stuff in column-major format
    colMajorXform((short*) wipe_scr_start, width / 2, height);
    colMajorXform((short*) wipe_scr_end, width / 2, height);

    // setup initial column positions
    // (wipe_melt_offsets<0 => not ready to scroll yet)
    wipe_melt_offsets = (int*) Z_Malloc(width * sizeof(int), PU_STATIC, 0);
    wipe_melt_offsets[0] = -(M_Random() % 16);
    for (i = 1; i < width; i++)
    {
        r = (M_Random() % 3) - 1;
        wipe_melt_offsets[i] = wipe_melt_offsets[i - 1] + r;
        if (wipe_melt_offsets[i] > 0)
            wipe_melt_offsets[i] = 0;
        else if (wipe_melt_offsets[i] == -16)
            wipe_melt_offsets[i] = -15;
    }

    return 0;
}

int doMelt(int width, int height, int ticks)
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
                wipe_melt_offsets[i]++;
                done = false;
            }
            else if (wipe_melt_offsets[i] < height)
            {
                dy = (wipe_melt_offsets[i] < 16) ? wipe_melt_offsets[i] + 1 : 8;
                if (wipe_melt_offsets[i] + dy >= height)
                    dy = height - wipe_melt_offsets[i];
                s = &((short*) wipe_scr_end)[i * height + wipe_melt_offsets[i]];
                d = &((short*) wipe_scr)[wipe_melt_offsets[i] * width + i];
                idx = 0;
                for (j = dy; j; j--)
                {
                    d[idx] = *(s++);
                    idx += width;
                }
                wipe_melt_offsets[i] += dy;
                s = &((short*) wipe_scr_start)[i * height];
                d = &((short*) wipe_scr)[wipe_melt_offsets[i] * width + i];
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

int exitMelt(int width, int height, int ticks)
{
    (void) width;
    (void) height;
    (void) ticks;
    Z_Free(wipe_melt_offsets);
    return 0;
}

int startScreen(int x, int y, int width, int height)
{
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    wipe_scr_start = screens[2];
    I_ReadScreen(wipe_scr_start);
    return 0;
}

int endScreen(int x, int y, int width, int height)
{
    wipe_scr_end = screens[3];
    I_ReadScreen(wipe_scr_end);
    V_DrawBlock(x, y, 0, width, height, wipe_scr_start); // restore start scr.
    return 0;
}

int screenWipe(int wipeno, int x, int y, int width, int height, int ticks)
{
    (void) x;
    (void) y;
    int rc;
    static int (*wipes[])(int, int, int) = {
        initColorXForm, doColorXForm, exitColorXForm, initMelt, doMelt, exitMelt};

    // initial stuff
    if (!wipe_melt_running)
    {
        wipe_melt_running = 1;
        wipe_scr = screens[0];
        (*wipes[wipeno * 3])(width, height, ticks);
    }

    // do a piece of wipe-in
    V_MarkRect(0, 0, width, height);
    rc = (*wipes[wipeno * 3 + 1])(width, height, ticks);

    // final stuff
    if (rc)
    {
        wipe_melt_running = 0;
        (*wipes[wipeno * 3 + 2])(width, height, ticks);
    }

    return !wipe_melt_running;
}

} // namespace Doom
