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

#include "../f_wipe.h" // the shim's globals (wipe_scr_start / offsets / running)
#include "Wipe.h"
#include "WipeState.h"

#include "../Render/Video.h"
#include <ea_data_structures/Structures/Vector.h>

#include "../Host/Video.h"
#include "../Sim/Random.h"
namespace Doom
{

// The melt's scratch framebuffers now live on the Engine (UI/WipeState.h, moved by the
// file-scope-statics sweep - REFACTOR.md, Step 5). The vanilla names are references onto that
// member. The exported wipe_scr_start / wipe_melt_offsets / wipe_melt_running stay in the f_wipe.cpp
// shim, being what the GPU compositor reads.
static byte*& wipe_scr_end = wipeState().wipe_scr_end;
static byte*& wipe_scr = wipeState().wipe_scr;

void colMajorXform(short* array, int width, int height)
{
    // RAII scratch: the transposed copy, released when the function returns.
    auto dest = EA::Vector<short>(width * height);

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            dest[x * height + y] = array[y * width + x];

    doom_memcpy(array, dest.data(), width * height * 2);
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
    int r;
    (void) ticks;

    // copy start screen to main screen
    doom_memcpy(wipe_scr, wipe_scr_start, width * height);

    // makes this wipe faster (in theory)
    // to have stuff in column-major format
    colMajorXform(reinterpret_cast<short*>(wipe_scr_start), width / 2, height);
    colMajorXform(reinterpret_cast<short*>(wipe_scr_end), width / 2, height);

    // setup initial column positions
    // (wipe_melt_offsets<0 => not ready to scroll yet)
    wipe_melt_offsets = static_cast<int*>(doom_malloc(width * sizeof(int)));
    wipe_melt_offsets[0] = -(Doom::randomness().forMenu() % 16);
    for (int i = 1; i < width; i++)
    {
        r = (Doom::randomness().forMenu() % 3) - 1;
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
    int dy;
    int idx;

    short* s;
    short* d;
    doom_boolean done = true;

    width /= 2;

    while (ticks--)
    {
        for (int i = 0; i < width; i++)
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
                s = &(reinterpret_cast<short*>(
                    wipe_scr_end))[i * height + wipe_melt_offsets[i]];
                d = &(reinterpret_cast<short*>(
                    wipe_scr))[wipe_melt_offsets[i] * width + i];
                idx = 0;
                for (int j = dy; j; j--)
                {
                    d[idx] = *(s++);
                    idx += width;
                }
                wipe_melt_offsets[i] += dy;
                s = &(reinterpret_cast<short*>(wipe_scr_start))[i * height];
                d = &(reinterpret_cast<short*>(
                    wipe_scr))[wipe_melt_offsets[i] * width + i];
                idx = 0;
                for (int j = height - wipe_melt_offsets[i]; j; j--)
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
    doom_free(wipe_melt_offsets);
    return 0;
}

int startScreen(int x, int y, int width, int height)
{
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    wipe_scr_start = screens[2];
    readScreen(wipe_scr_start);
    return 0;
}

int endScreen(int x, int y, int width, int height)
{
    wipe_scr_end = screens[3];
    readScreen(wipe_scr_end);
    Doom::drawBlock(x, y, 0, width, height, wipe_scr_start); // restore start scr.
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
    Doom::markRect(0, 0, width, height);
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
