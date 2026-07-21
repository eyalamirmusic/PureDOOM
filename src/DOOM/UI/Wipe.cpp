// Rewritten out of vanilla f_wipe into namespace Doom.
//
// The mission-begin melt/wipe. f_wipe.cpp shims the three wipe_ entry points and
// owns the state a GPU compositor reads (wipe_melt_running, wipe_scr_start, the
// column offsets); the scratch screens and the melt/colour-transform helpers are
// file-local here. The melt walks M_Random (not P_Random), so it never desyncs
// the simulation. Golden-neutral.

#include "../Host/Platform.h"
#include "../doomtype.h"

#include "../Game/GameDefs.h"
#include "../Sim/Random.h"

#include "Wipe.h" // the shim's globals (wipe_scr_start / offsets / running)
#include "Wipe.h"
#include "WipeState.h"

#include "../Render/Video.h"
#include "../Containers.h"

#include "../Host/Video.h"
#include "../Sim/Random.h"
namespace Doom
{

// The melt's scratch framebuffers and the per-column offset table live on the Engine
// (UI/WipeState.h). Every function below reaches them through a hoisted
// `auto& scratch = wipeState();` local instead of a file-scope alias (REFACTOR.md, Step 9
// strand (a)). wipe_scr_start / wipe_melt_running stay in the f_wipe.cpp shim outright;
// wipe_melt_offsets stays there too, but as a view refreshed by initMelt - all three are
// what the GPU compositor reads.

void colMajorXform(short* array, int width, int height)
{
    // RAII scratch: the transposed copy, released when the function returns.
    auto dest = Vector<short>(width * height);

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            dest[x * height + y] = array[y * width + x];

    doom_memcpy(array, dest.data(), width * height * 2);
}

int initColorXForm(int width, int height, [[maybe_unused]] int ticks)
{
    doom_memcpy(wipeState().wipe_scr, wipe_scr_start, width * height);
    return 0;
}

int doColorXForm(int width, int height, int ticks)
{
    auto& scratch = wipeState();

    int newval;

    bool changed = false;
    byte* w = scratch.wipe_scr;
    byte* e = scratch.wipe_scr_end;

    while (w != scratch.wipe_scr + width * height)
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

int exitColorXForm([[maybe_unused]] int width,
                   [[maybe_unused]] int height,
                   [[maybe_unused]] int ticks)
{
    return 0;
}

int initMelt(int width, int height, [[maybe_unused]] int ticks)
{
    auto& scratch = wipeState();

    // copy start screen to main screen
    doom_memcpy(scratch.wipe_scr, wipe_scr_start, width * height);

    // makes this wipe faster (in theory)
    // to have stuff in column-major format
    colMajorXform(reinterpret_cast<short*>(wipe_scr_start), width / 2, height);
    colMajorXform(reinterpret_cast<short*>(scratch.wipe_scr_end), width / 2, height);

    // setup initial column positions
    // (wipe_melt_offsets<0 => not ready to scroll yet)
    // RAII-owned (Step 9): scratch.wipe_melt_offsets is the owning vector; the
    // vanilla name wipe_melt_offsets is a view refreshed here after the resize.
    scratch.wipe_melt_offsets.resize(width);
    wipe_melt_offsets = scratch.wipe_melt_offsets.data();
    wipe_melt_offsets[0] = -(randomness().forMenu() % 16);
    for (int i = 1; i < width; i++)
    {
        int r = (randomness().forMenu() % 3) - 1;
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
    auto& scratch = wipeState();

    bool done = true;

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
                int dy = (wipe_melt_offsets[i] < 16) ? wipe_melt_offsets[i] + 1 : 8;
                if (wipe_melt_offsets[i] + dy >= height)
                    dy = height - wipe_melt_offsets[i];
                short* s = &(reinterpret_cast<short*>(
                    scratch.wipe_scr_end))[i * height + wipe_melt_offsets[i]];
                short* d = &(reinterpret_cast<short*>(
                    scratch.wipe_scr))[wipe_melt_offsets[i] * width + i];
                int idx = 0;
                for (int j = dy; j; j--)
                {
                    d[idx] = *(s++);
                    idx += width;
                }
                wipe_melt_offsets[i] += dy;
                s = &(reinterpret_cast<short*>(wipe_scr_start))[i * height];
                d = &(reinterpret_cast<short*>(
                    scratch.wipe_scr))[wipe_melt_offsets[i] * width + i];
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

int exitMelt([[maybe_unused]] int width,
             [[maybe_unused]] int height,
             [[maybe_unused]] int ticks)
{
    // Clears the owning vector; the view (wipe_melt_offsets) is deliberately left
    // unrefreshed, reproducing vanilla's "freed but not nulled" pointer - see the
    // comment on WipeState::wipe_melt_offsets.
    wipeState().wipe_melt_offsets.clear();
    return 0;
}

int startScreen([[maybe_unused]] int x,
                [[maybe_unused]] int y,
                [[maybe_unused]] int width,
                [[maybe_unused]] int height)
{
    wipe_scr_start = screens[2];
    readScreen(wipe_scr_start);
    return 0;
}

int endScreen(int x, int y, int width, int height)
{
    auto& scratch = wipeState();

    scratch.wipe_scr_end = screens[3];
    readScreen(scratch.wipe_scr_end);
    drawBlock(x, y, 0, width, height, wipe_scr_start); // restore start scr.
    return 0;
}

int screenWipe(WipeType wipeno,
               [[maybe_unused]] int x,
               [[maybe_unused]] int y,
               int width,
               int height,
               int ticks)
{
    static int (*wipes[])(int, int, int) = {
        initColorXForm, doColorXForm, exitColorXForm, initMelt, doMelt, exitMelt};

    // initial stuff
    if (!wipe_melt_running)
    {
        wipe_melt_running = true;
        wipeState().wipe_scr = screens[0];
        (*wipes[toIndex(wipeno) * 3])(width, height, ticks);
    }

    // do a piece of wipe-in
    markRect(0, 0, width, height);
    int rc = (*wipes[toIndex(wipeno) * 3 + 1])(width, height, ticks);

    // final stuff
    if (rc)
    {
        wipe_melt_running = false;
        (*wipes[toIndex(wipeno) * 3 + 2])(width, height, ticks);
    }

    return !wipe_melt_running;
}

} // namespace Doom

// ---------------------------------------------------------------------------
// Global-scope data that was f_wipe.cpp. It stays at :: scope because these are the
// vanilla names other translation units (and the eacp port) still link against.
// ---------------------------------------------------------------------------
//
// SCREEN WIPE PACKAGE
//

// Raised while a melt is running, and the only safe thing to test: exitMelt clears
// the column table's owning vector without refreshing the pointer to it. (Read by
// the GPU melt compositor in EngineAccess.)
bool wipe_melt_running = false;

// The outgoing frame, as palette indices; wipe_initMelt leaves it column-major.
byte* wipe_scr_start;

// How far down each two-pixel column of the outgoing screen has slid so far.
// Negative means the column has not started moving yet. RAII-owned (Step 9): this is
// now a VIEW onto WipeState::wipe_melt_offsets.data(), refreshed by initMelt; see
// UI/WipeState.h.
int* wipe_melt_offsets;
