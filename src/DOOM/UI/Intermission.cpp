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
//        Intermission screens.
//
//-----------------------------------------------------------------------------

// Rewritten out of vanilla wi_stuff into namespace Doom.
//
// The level-completion intermission: the single-player stat count-up, the
// deathmatch/coop tables, and the animated backgrounds and "you are here"
// pointer. wi_stuff.cpp shims the three WI_ names. Every intermission global is
// file-local here (all were file-static already), so there is nothing to own in
// the shim. No attract demo reaches the intermission, so this is a faithful
// transcription.

#include "../Host/Diagnostics.h"
#include "../Host/Platform.h"
#include "../Host/Text.h"

#include "../Game/MapSpawns.h"
#include "../Sim/Random.h"
#include "../Math/Swap.h"
#include "../Game/SoundData.h" // Data.
#include "../Wad/WadFile.h"
#include "IntermissionTypes.h"

#include "Intermission.h"
#include "IntermissionState.h"

#include "../Render/Video.h"
#include "../Containers.h"

#include "../Game/Game.h"
#include "../Game/GameSession.h"
#include "../Game/GameVersion.h"
#include "../Game/PlayerState.h"
#include "../Game/Sound.h"
#include "../Sim/Random.h"

#include <algorithm>
namespace Doom
{

//
// Data needed to add patches to full screen intermission pics.
// Patches are statistics messages, and animations.
// Loads of by-pixel layout and placement, offsets etc.
//

//
// Different vetween registered DOOM (1994) and
//  Ultimate DOOM - Final edition (retail, 1995?).
// This is supposedly ignored for commercial
//  release (aka DOOM II), which had 34 maps
//  in one episode. So there.
constexpr int NUMEPISODES = 4;
constexpr int NUMMAPS = 9;

// GLOBAL LOCATIONS
constexpr int WI_TITLEY = 2;
constexpr int WI_SPACINGY = 33;

// SINGPLE-PLAYER STUFF
constexpr int SP_STATSX = 50;
constexpr int SP_STATSY = 50;

constexpr int SP_TIMEX = 16;
constexpr int SP_TIMEY = SCREENHEIGHT - 32;

// NET GAME STUFF
constexpr int NG_STATSY = 50;
constexpr int NG_SPACINGX = 64;

// DEATHMATCH STUFF
constexpr int DM_MATRIXX = 42;
constexpr int DM_MATRIXY = 68;

constexpr int DM_SPACINGX = 40;

constexpr int DM_TOTALSX = 269;

constexpr int DM_KILLERSX = 10;
constexpr int DM_KILLERSY = 100;
constexpr int DM_VICTIMSX = 5;
constexpr int DM_VICTIMSY = 50;

constexpr int FB = 0;

// States for single-player
#define SP_KILLS 0
#define SP_ITEMS 2
#define SP_SECRET 4
#define SP_FRAGS 6
#define SP_TIME 8
#define SP_PAR ST_TIME

#define SP_PAUSE 1

// in seconds
constexpr int SHOWNEXTLOCDELAY = 4;
//#define SHOWLASTLOCDELAY        SHOWNEXTLOCDELAY

enum AnimEnum
{
    ANIM_ALWAYS,
    ANIM_RANDOM,
    ANIM_LEVEL
};

struct Point
{
    int x;
    int y;
};

//
// Animation.
// There is another SurfaceAnim used in p_spec.
//
struct anim_t_wi_stuff
{
    AnimEnum type;

    // period in tics between animations
    int period;

    // number of animation frames
    int nanims;

    // location of animation
    Point loc;

    // ALWAYS: n/a,
    // RANDOM: period deviation (<256),
    // LEVEL: level
    int data1;

    // ALWAYS: n/a,
    // RANDOM: random base period,
    // LEVEL: n/a
    int data2;

    // actual graphics for frames of animations
    Patch* p[3];

    // following must be initialized to zero before use!

    // next value of bcnt (used in conjunction with period)
    int nexttic;

    // last drawn animation frame
    int lastdrawn;

    // next frame number to animate
    int ctr;

    // used by RANDOM and LEVEL when animating
    int state;
};

static Array<Array<Point, NUMMAPS>, NUMEPISODES> lnodes = {
    // Episode 0 World Map
    {
        {185, 164}, // location of level 0 (CJ)
        {148, 143}, // location of level 1 (CJ)
        {69, 122}, // location of level 2 (CJ)
        {209, 102}, // location of level 3 (CJ)
        {116, 89}, // location of level 4 (CJ)
        {166, 55}, // location of level 5 (CJ)
        {71, 56}, // location of level 6 (CJ)
        {135, 29}, // location of level 7 (CJ)
        {71, 24} // location of level 8 (CJ)
    },

    // Episode 1 World Map should go here
    {
        {254, 25}, // location of level 0 (CJ)
        {97, 50}, // location of level 1 (CJ)
        {188, 64}, // location of level 2 (CJ)
        {128, 78}, // location of level 3 (CJ)
        {214, 92}, // location of level 4 (CJ)
        {133, 130}, // location of level 5 (CJ)
        {208, 136}, // location of level 6 (CJ)
        {148, 140}, // location of level 7 (CJ)
        {235, 158} // location of level 8 (CJ)
    },

    // Episode 2 World Map should go here
    {
        {156, 168}, // location of level 0 (CJ)
        {48, 154}, // location of level 1 (CJ)
        {174, 95}, // location of level 2 (CJ)
        {265, 75}, // location of level 3 (CJ)
        {130, 48}, // location of level 4 (CJ)
        {279, 23}, // location of level 5 (CJ)
        {198, 48}, // location of level 6 (CJ)
        {140, 25}, // location of level 7 (CJ)
        {281, 136} // location of level 8 (CJ)
    }};

//
// Animation locations for episode 0 (1).
// Using patches saves a lot of space,
//  as they replace 320x200 full screen frames.
//
// These tables deliberately leave the per-frame animation fields (p, nexttic,
// ctr, ...) uninitialised; the struct comment above says they "must be
// initialized to zero before use", which aggregate init does. Silence the
// -Wmissing-field-initializers that legitimate 1993 idiom raises.
DOOM_DIAGNOSTIC_PUSH
DOOM_IGNORE_MISSING_FIELD_INITIALIZERS
static Array<anim_t_wi_stuff, 10> epsd0animinfo = {
    {ANIM_ALWAYS, TICRATE / 3, 3, {224, 104}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {184, 160}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {112, 136}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {72, 112}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {88, 96}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {64, 48}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {192, 40}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {136, 16}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {80, 16}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {64, 24}}};

static Array<anim_t_wi_stuff, 9> epsd1animinfo = {
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 1},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 2},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 3},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 4},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 5},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 6},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 7},
    {ANIM_LEVEL, TICRATE / 3, 3, {192, 144}, 8},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 8}};

static Array<anim_t_wi_stuff, 6> epsd2animinfo = {
    {ANIM_ALWAYS, TICRATE / 3, 3, {104, 168}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {40, 136}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {160, 96}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {104, 80}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {120, 32}},
    {ANIM_ALWAYS, TICRATE / 4, 3, {40, 0}}};
DOOM_DIAGNOSTIC_POP

static Array<int, NUMEPISODES> NUMANIMS = {
    epsd0animinfo.size(), epsd1animinfo.size(), epsd2animinfo.size()};

static Array<anim_t_wi_stuff*, NUMEPISODES> anims_wi_stuff = {
    epsd0animinfo.data(), epsd1animinfo.data(), epsd2animinfo.data()};

//
// GENERAL DATA
//

// The intermission's residual runtime state and loaded graphics live on the Engine
// (UI/IntermissionState.h, moved by the file-scope-statics sweep - REFACTOR.md, Step 5). They used
// to be reached through file-scope `static T& x = intermissionState().x;` reference aliases; the
// file-local-alias sweep (REFACTOR.md, Step 9 strand (a)) retired them - every function below
// reaches intermissionState() through a hoisted local instead, taken once per function. The
// animation/layout data tables above (lnodes / epsd*animinfo / NUMANIMS / anims_wi_stuff) stay
// file-local: anims_wi_stuff points into epsd*animinfo, a self-referential-pointer table that does
// not survive being a copyable struct member (the trap AutomapView's cheat sequence documented).

//
// CODE
//

void slamBackground()
{
    doom_memcpy(screens[0], screens[1], SCREENWIDTH * SCREENHEIGHT);
    markRect(0, 0, SCREENWIDTH, SCREENHEIGHT);
}

// The ticker is used to detect keys
//  because of timing issues in netgames.
bool intermissionResponder(Event*)
{
    return false;
}

// Draws "<Levelname> Finished!"
void drawLF()
{
    auto& im = intermissionState();

    int y = WI_TITLEY;

    // draw <LevelName>
    drawPatch((SCREENWIDTH - littleEndian(im.lnames[im.wbs->last]->width)) / 2,
              y,
              FB,
              im.lnames[im.wbs->last]);

    // draw "Finished!"
    y += (5 * littleEndian(im.lnames[im.wbs->last]->height)) / 4;

    drawPatch(
        (SCREENWIDTH - littleEndian(im.finished->width)) / 2, y, FB, im.finished);
}

// Draws "Entering <LevelName>"
void drawEL()
{
    auto& im = intermissionState();

    int y = WI_TITLEY;

    // draw "Entering"
    drawPatch(
        (SCREENWIDTH - littleEndian(im.entering->width)) / 2, y, FB, im.entering);

    // draw level
    y += (5 * littleEndian(im.lnames[im.wbs->next]->height)) / 4;

    drawPatch((SCREENWIDTH - littleEndian(im.lnames[im.wbs->next]->width)) / 2,
              y,
              FB,
              im.lnames[im.wbs->next]);
}

void drawOnLnode(int n, Patch* c[])
{
    auto& im = intermissionState();

    bool fits = false;

    int i = 0;
    do
    {
        int left = lnodes[im.wbs->epsd][n].x - littleEndian(c[i]->leftoffset);
        int top = lnodes[im.wbs->epsd][n].y - littleEndian(c[i]->topoffset);
        int right = left + littleEndian(c[i]->width);
        int bottom = top + littleEndian(c[i]->height);

        if (left >= 0 && right < SCREENWIDTH && top >= 0 && bottom < SCREENHEIGHT)
        {
            fits = true;
        }
        else
        {
            i++;
        }
    } while (!fits && i != 2);

    if (fits && i < 2)
    {
        drawPatch(lnodes[im.wbs->epsd][n].x, lnodes[im.wbs->epsd][n].y, FB, c[i]);
    }
    else
    {
        // DEBUG
        //doom_print("Could not place patch on level %d", n + 1);
        print("Could not place patch on level ", n + 1);
    }
}

void initAnimatedBack()
{
    auto& im = intermissionState();

    if (gameVersion().gamemode == commercial)
        return;

    if (im.wbs->epsd > 2)
        return;

    for (int i = 0; i < NUMANIMS[im.wbs->epsd]; i++)
    {
        anim_t_wi_stuff* a = &anims_wi_stuff[im.wbs->epsd][i];

        // init variables
        a->ctr = -1;

        // specify the next time to draw it
        if (a->type == ANIM_ALWAYS)
            a->nexttic = im.bcnt + 1 + (randomness().forMenu() % a->period);
        else if (a->type == ANIM_RANDOM)
            a->nexttic =
                im.bcnt + 1 + a->data2 + (randomness().forMenu() % a->data1);
        else if (a->type == ANIM_LEVEL)
            a->nexttic = im.bcnt + 1;
    }
}

void updateAnimatedBack()
{
    auto& im = intermissionState();

    if (gameVersion().gamemode == commercial)
        return;

    if (im.wbs->epsd > 2)
        return;

    for (int i = 0; i < NUMANIMS[im.wbs->epsd]; i++)
    {
        anim_t_wi_stuff* a = &anims_wi_stuff[im.wbs->epsd][i];

        if (im.bcnt == a->nexttic)
        {
            switch (a->type)
            {
                case ANIM_ALWAYS:
                    if (++a->ctr >= a->nanims)
                        a->ctr = 0;
                    a->nexttic = im.bcnt + a->period;
                    break;

                case ANIM_RANDOM:
                    a->ctr++;
                    if (a->ctr == a->nanims)
                    {
                        a->ctr = -1;
                        a->nexttic =
                            im.bcnt + a->data2 + (randomness().forMenu() % a->data1);
                    }
                    else
                        a->nexttic = im.bcnt + a->period;
                    break;

                case ANIM_LEVEL:
                    // gawd-awful hack for level anims
                    if (!(im.state == StatCount && i == 7)
                        && im.wbs->next == a->data1)
                    {
                        a->ctr++;
                        if (a->ctr == a->nanims)
                            a->ctr--;
                        a->nexttic = im.bcnt + a->period;
                    }
                    break;
            }
        }
    }
}

void drawAnimatedBack()
{
    auto& im = intermissionState();

    anim_t_wi_stuff* a;

    // Preserved exactly as found, bug included. This tests the enum *constant*
    // `commercial` - which is 2, and therefore always true - rather than
    // `gameVersion().gamemode == commercial`, so the function returns before
    // drawing anything in every game mode. The intermission's animated background
    // has consequently never drawn in this lineage; the identical line is in the
    // 1993-lineage source (110ddbe:src/DOOM/wi_stuff.c:562). The frame goldens are
    // recorded with it, so correcting it is a behaviour change, not a cleanup.
    // Written as an explicit comparison only so the always-true test is visible
    // rather than something a compiler has to point out.
    if (commercial != 0)
        return;

    if (im.wbs->epsd > 2)
        return;

    for (int i = 0; i < NUMANIMS[im.wbs->epsd]; i++)
    {
        a = &anims_wi_stuff[im.wbs->epsd][i];

        if (a->ctr >= 0)
            drawPatch(a->loc.x, a->loc.y, FB, a->p[a->ctr]);
    }
}

//
// Draws a number.
// If digits > 0, then use that many digits minimum,
//  otherwise only use as many as necessary.
// Returns new x position.
//
int drawIntermissionNum(int x, int y, int n, int digits)
{
    auto& im = intermissionState();

    int fontwidth = littleEndian(im.num[0]->width);

    if (digits < 0)
    {
        if (!n)
        {
            // make variable-length zeros 1 digit long
            digits = 1;
        }
        else
        {
            // figure out # of digits in #
            digits = 0;
            int temp = n;

            while (temp)
            {
                temp /= 10;
                digits++;
            }
        }
    }

    int neg = n < 0;
    if (neg)
        n = -n;

    // if non-number, do not draw it
    if (n == 1994)
        return 0;

    // draw the new number
    while (digits--)
    {
        x -= fontwidth;
        drawPatch(x, y, FB, im.num[n % 10]);
        n /= 10;
    }

    // draw a minus sign if necessary
    if (neg)
        drawPatch(x -= 8, y, FB, im.wiminus);

    return x;
}

void drawPercent(int x, int y, int p)
{
    if (p < 0)
        return;

    drawPatch(x, y, FB, intermissionState().percent);
    drawIntermissionNum(x, y, p, -1);
}

//
// Display level completion time and par,
//  or "sucks" message if overflow.
//
void drawTime(int x, int y, int t)
{
    auto& im = intermissionState();

    if (t < 0)
        return;

    if (t <= 61 * 59)
    {
        int div = 1;

        do
        {
            int n = (t / div) % 60;
            x = drawIntermissionNum(x, y, n, 2) - littleEndian(im.colon->width);
            div *= 60;

            // draw
            if (div == 60 || t / div)
                drawPatch(x, y, FB, im.colon);

        } while (t / div);
    }
    else
    {
        // "sucks"
        drawPatch(x - littleEndian(im.sucks->width), y, FB, im.sucks);
    }
}

// Defined below, after the loader it undoes. Declared here at namespace scope
// rather than inside endIntermission's body, which is where vanilla put it: a
// block-scope `extern` function declaration is a 1993 C idiom that C++ still
// accepts but resolves by a rule nobody should have to reason about, and MSVC
// resolves it the other way - it took the name to be ::unloadIntermissionData
// and left the reference unresolved at link time, so the whole test suite failed
// to link on that toolchain alone. The declaration belongs where the definition
// lives, and then every compiler agrees.
void unloadIntermissionData();

void endIntermission()
{
    unloadIntermissionData();
}

void initNoState()
{
    auto& im = intermissionState();

    im.state = NoState;
    im.acceleratestage = 0;
    im.cnt = 10;
}

void updateNoState()
{
    updateAnimatedBack();

    if (!--intermissionState().cnt)
    {
        endIntermission();
        worldDone();
    }
}

void initShowNextLoc()
{
    auto& im = intermissionState();

    im.state = ShowNextLoc;
    im.acceleratestage = 0;
    im.cnt = SHOWNEXTLOCDELAY * TICRATE;

    initAnimatedBack();
}

void updateShowNextLoc()
{
    auto& im = intermissionState();

    updateAnimatedBack();

    if (!--im.cnt || im.acceleratestage)
        initNoState();
    else
        im.snl_pointeron = (im.cnt & 31) < 20;
}

void drawShowNextLoc()
{
    auto& im = intermissionState();
    const auto& version = gameVersion();

    slamBackground();

    // draw animated background
    drawAnimatedBack();

    if (version.gamemode != commercial)
    {
        if (im.wbs->epsd > 2)
        {
            drawEL();
            return;
        }

        int last = (im.wbs->last == 8) ? im.wbs->next - 1 : im.wbs->last;

        // draw a splat on taken cities.
        for (int i = 0; i <= last; i++)
            drawOnLnode(i, &im.splat);

        // splat the secret level?
        if (im.wbs->didsecret)
            drawOnLnode(8, &im.splat);

        // draw flashing ptr
        if (im.snl_pointeron)
            drawOnLnode(im.wbs->next, im.yah.data());
    }

    // draws which level you are entering..
    if ((version.gamemode != commercial) || im.wbs->next != 30)
        drawEL();
}

void drawNoState()
{
    intermissionState().snl_pointeron = true;
    drawShowNextLoc();
}

int fragSum(int playernum)
{
    auto& im = intermissionState();

    int frags = 0;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (playerState().playeringame[i] && i != playernum)
        {
            frags += im.plrs[playernum].frags[i];
        }
    }

    // JDC hack - negative frags.
    frags -= im.plrs[playernum].frags[playernum];
    // UNUSED if (frags < 0)
    //         frags = 0;

    return frags;
}

void initDeathmatchStats()
{
    auto& im = intermissionState();
    const auto& players_ = playerState();

    im.state = StatCount;
    im.acceleratestage = 0;
    im.dm_state = 1;

    im.cnt_pause = TICRATE;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (players_.playeringame[i])
        {
            for (int j = 0; j < MAXPLAYERS; j++)
                if (players_.playeringame[j])
                    im.dm_frags[i][j] = 0;

            im.dm_totals[i] = 0;
        }
    }

    initAnimatedBack();
}

void updateDeathmatchStats()
{
    auto& im = intermissionState();
    const auto& players_ = playerState();

    updateAnimatedBack();

    if (im.acceleratestage && im.dm_state != 4)
    {
        im.acceleratestage = 0;

        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (players_.playeringame[i])
            {
                for (int j = 0; j < MAXPLAYERS; j++)
                    if (players_.playeringame[j])
                        im.dm_frags[i][j] = im.plrs[i].frags[j];

                im.dm_totals[i] = fragSum(i);
            }
        }

        startSound(nullptr, sfx_barexp);
        im.dm_state = 4;
    }

    if (im.dm_state == 2)
    {
        if (!(im.bcnt & 3))
            startSound(nullptr, sfx_pistol);

        bool stillticking = false;

        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (players_.playeringame[i])
            {
                for (int j = 0; j < MAXPLAYERS; j++)
                {
                    if (players_.playeringame[j]
                        && im.dm_frags[i][j] != im.plrs[i].frags[j])
                    {
                        if (im.plrs[i].frags[j] < 0)
                            im.dm_frags[i][j]--;
                        else
                            im.dm_frags[i][j]++;

                        im.dm_frags[i][j] = std::clamp(im.dm_frags[i][j], -99, 99);

                        stillticking = true;
                    }
                }
                im.dm_totals[i] = fragSum(i);

                im.dm_totals[i] = std::clamp(im.dm_totals[i], -99, 99);
            }
        }
        if (!stillticking)
        {
            startSound(nullptr, sfx_barexp);
            im.dm_state++;
        }
    }
    else if (im.dm_state == 4)
    {
        if (im.acceleratestage)
        {
            startSound(nullptr, sfx_slop);

            if (gameVersion().gamemode == commercial)
                initNoState();
            else
                initShowNextLoc();
        }
    }
    else if (im.dm_state & 1)
    {
        if (!--im.cnt_pause)
        {
            im.dm_state++;
            im.cnt_pause = TICRATE;
        }
    }
}

void drawDeathmatchStats()
{
    auto& im = intermissionState();
    const auto& players_ = playerState();

    slamBackground();

    // draw animated background
    drawAnimatedBack();
    drawLF();

    // draw stat titles (top line)
    drawPatch(DM_TOTALSX - littleEndian(im.total->width) / 2,
              DM_MATRIXY - WI_SPACINGY + 10,
              FB,
              im.total);

    drawPatch(DM_KILLERSX, DM_KILLERSY, FB, im.killers);
    drawPatch(DM_VICTIMSX, DM_VICTIMSY, FB, im.victims);

    // draw P?
    int x = DM_MATRIXX + DM_SPACINGX;
    int y = DM_MATRIXY;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (players_.playeringame[i])
        {
            drawPatch(x - littleEndian(im.p[i]->width) / 2,
                      DM_MATRIXY - WI_SPACINGY,
                      FB,
                      im.p[i]);

            drawPatch(DM_MATRIXX - littleEndian(im.p[i]->width) / 2, y, FB, im.p[i]);

            if (i == im.me)
            {
                drawPatch(x - littleEndian(im.p[i]->width) / 2,
                          DM_MATRIXY - WI_SPACINGY,
                          FB,
                          im.bstar);

                drawPatch(
                    DM_MATRIXX - littleEndian(im.p[i]->width) / 2, y, FB, im.star);
            }
        }
        else
        {
            // Doom::drawPatch(x-littleEndian(bp[i]->width)/2,
            //   DM_MATRIXY - WI_SPACINGY, FB, bp[i]);
            // Doom::drawPatch(DM_MATRIXX-littleEndian(bp[i]->width)/2,
            //   y, FB, bp[i]);
        }
        x += DM_SPACINGX;
        y += WI_SPACINGY;
    }

    // draw stats
    y = DM_MATRIXY + 10;
    int w = littleEndian(im.num[0]->width);

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        x = DM_MATRIXX + DM_SPACINGX;

        if (players_.playeringame[i])
        {
            for (int j = 0; j < MAXPLAYERS; j++)
            {
                if (players_.playeringame[j])
                    drawIntermissionNum(x + w, y, im.dm_frags[i][j], 2);

                x += DM_SPACINGX;
            }
            drawIntermissionNum(DM_TOTALSX + w, y, im.dm_totals[i], 2);
        }
        y += WI_SPACINGY;
    }
}

void initNetgameStats()
{
    auto& im = intermissionState();

    im.state = StatCount;
    im.acceleratestage = 0;
    im.ng_state = 1;

    im.cnt_pause = TICRATE;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (!playerState().playeringame[i])
            continue;

        im.cnt_kills[i] = im.cnt_items[i] = im.cnt_secret[i] = im.cnt_frags[i] = 0;

        im.dofrags += fragSum(i);
    }

    im.dofrags = !!im.dofrags;

    initAnimatedBack();
}

void updateNetgameStats()
{
    auto& im = intermissionState();
    const auto& players_ = playerState();

    int fsum;

    bool stillticking;

    updateAnimatedBack();

    if (im.acceleratestage && im.ng_state != 10)
    {
        im.acceleratestage = 0;

        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (!players_.playeringame[i])
                continue;

            im.cnt_kills[i] = (im.plrs[i].skills * 100) / im.wbs->maxkills;
            im.cnt_items[i] = (im.plrs[i].sitems * 100) / im.wbs->maxitems;
            im.cnt_secret[i] = (im.plrs[i].ssecret * 100) / im.wbs->maxsecret;

            if (im.dofrags)
                im.cnt_frags[i] = fragSum(i);
        }
        startSound(nullptr, sfx_barexp);
        im.ng_state = 10;
    }

    if (im.ng_state == 2)
    {
        if (!(im.bcnt & 3))
            startSound(nullptr, sfx_pistol);

        stillticking = false;

        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (!players_.playeringame[i])
                continue;

            im.cnt_kills[i] += 2;

            if (im.cnt_kills[i] >= (im.plrs[i].skills * 100) / im.wbs->maxkills)
                im.cnt_kills[i] = (im.plrs[i].skills * 100) / im.wbs->maxkills;
            else
                stillticking = true;
        }

        if (!stillticking)
        {
            startSound(nullptr, sfx_barexp);
            im.ng_state++;
        }
    }
    else if (im.ng_state == 4)
    {
        if (!(im.bcnt & 3))
            startSound(nullptr, sfx_pistol);

        stillticking = false;

        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (!players_.playeringame[i])
                continue;

            im.cnt_items[i] += 2;
            if (im.cnt_items[i] >= (im.plrs[i].sitems * 100) / im.wbs->maxitems)
                im.cnt_items[i] = (im.plrs[i].sitems * 100) / im.wbs->maxitems;
            else
                stillticking = true;
        }
        if (!stillticking)
        {
            startSound(nullptr, sfx_barexp);
            im.ng_state++;
        }
    }
    else if (im.ng_state == 6)
    {
        if (!(im.bcnt & 3))
            startSound(nullptr, sfx_pistol);

        stillticking = false;

        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (!players_.playeringame[i])
                continue;

            im.cnt_secret[i] += 2;

            if (im.cnt_secret[i] >= (im.plrs[i].ssecret * 100) / im.wbs->maxsecret)
                im.cnt_secret[i] = (im.plrs[i].ssecret * 100) / im.wbs->maxsecret;
            else
                stillticking = true;
        }

        if (!stillticking)
        {
            startSound(nullptr, sfx_barexp);
            im.ng_state += 1 + 2 * !im.dofrags;
        }
    }
    else if (im.ng_state == 8)
    {
        if (!(im.bcnt & 3))
            startSound(nullptr, sfx_pistol);

        stillticking = false;

        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (!players_.playeringame[i])
                continue;

            im.cnt_frags[i] += 1;

            if (im.cnt_frags[i] >= (fsum = fragSum(i)))
                im.cnt_frags[i] = fsum;
            else
                stillticking = true;
        }

        if (!stillticking)
        {
            startSound(nullptr, sfx_pldeth);
            im.ng_state++;
        }
    }
    else if (im.ng_state == 10)
    {
        if (im.acceleratestage)
        {
            startSound(nullptr, sfx_sgcock);
            if (gameVersion().gamemode == commercial)
                initNoState();
            else
                initShowNextLoc();
        }
    }
    else if (im.ng_state & 1)
    {
        if (!--im.cnt_pause)
        {
            im.ng_state++;
            im.cnt_pause = TICRATE;
        }
    }
}

void drawNetgameStats()
{
    auto& im = intermissionState();

    const int statsX = 32 + littleEndian(im.star->width) / 2 + 32 * !im.dofrags;

    int pwidth = littleEndian(im.percent->width);

    slamBackground();

    // draw animated background
    drawAnimatedBack();

    drawLF();

    // draw stat titles (top line)
    drawPatch(statsX + NG_SPACINGX - littleEndian(im.kills->width),
              NG_STATSY,
              FB,
              im.kills);

    drawPatch(statsX + 2 * NG_SPACINGX - littleEndian(im.items->width),
              NG_STATSY,
              FB,
              im.items);

    drawPatch(statsX + 3 * NG_SPACINGX - littleEndian(im.secret->width),
              NG_STATSY,
              FB,
              im.secret);

    if (im.dofrags)
        drawPatch(statsX + 4 * NG_SPACINGX - littleEndian(im.frags->width),
                  NG_STATSY,
                  FB,
                  im.frags);

    // draw stats
    int y = NG_STATSY + littleEndian(im.kills->height);

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (!playerState().playeringame[i])
            continue;

        int x = statsX;
        drawPatch(x - littleEndian(im.p[i]->width), y, FB, im.p[i]);

        if (i == im.me)
            drawPatch(x - littleEndian(im.p[i]->width), y, FB, im.star);

        x += NG_SPACINGX;
        drawPercent(x - pwidth, y + 10, im.cnt_kills[i]);
        x += NG_SPACINGX;
        drawPercent(x - pwidth, y + 10, im.cnt_items[i]);
        x += NG_SPACINGX;
        drawPercent(x - pwidth, y + 10, im.cnt_secret[i]);
        x += NG_SPACINGX;

        if (im.dofrags)
            drawIntermissionNum(x, y + 10, im.cnt_frags[i], -1);

        y += WI_SPACINGY;
    }
}

void initStats()
{
    auto& im = intermissionState();

    im.state = StatCount;
    im.acceleratestage = 0;
    im.sp_state = 1;
    im.cnt_kills[0] = im.cnt_items[0] = im.cnt_secret[0] = -1;
    im.cnt_time = im.cnt_par = -1;
    im.cnt_pause = TICRATE;

    initAnimatedBack();
}

void updateStats()
{
    auto& im = intermissionState();

    updateAnimatedBack();

    if (im.acceleratestage && im.sp_state != 10)
    {
        im.acceleratestage = 0;
        im.cnt_kills[0] = (im.plrs[im.me].skills * 100) / im.wbs->maxkills;
        im.cnt_items[0] = (im.plrs[im.me].sitems * 100) / im.wbs->maxitems;
        im.cnt_secret[0] = (im.plrs[im.me].ssecret * 100) / im.wbs->maxsecret;
        im.cnt_time = im.plrs[im.me].stime / TICRATE;
        im.cnt_par = im.wbs->partime / TICRATE;
        startSound(nullptr, sfx_barexp);
        im.sp_state = 10;
    }

    if (im.sp_state == 2)
    {
        im.cnt_kills[0] += 2;

        if (!(im.bcnt & 3))
            startSound(nullptr, sfx_pistol);

        if (im.cnt_kills[0] >= (im.plrs[im.me].skills * 100) / im.wbs->maxkills)
        {
            im.cnt_kills[0] = (im.plrs[im.me].skills * 100) / im.wbs->maxkills;
            startSound(nullptr, sfx_barexp);
            im.sp_state++;
        }
    }
    else if (im.sp_state == 4)
    {
        im.cnt_items[0] += 2;

        if (!(im.bcnt & 3))
            startSound(nullptr, sfx_pistol);

        if (im.cnt_items[0] >= (im.plrs[im.me].sitems * 100) / im.wbs->maxitems)
        {
            im.cnt_items[0] = (im.plrs[im.me].sitems * 100) / im.wbs->maxitems;
            startSound(nullptr, sfx_barexp);
            im.sp_state++;
        }
    }
    else if (im.sp_state == 6)
    {
        im.cnt_secret[0] += 2;

        if (!(im.bcnt & 3))
            startSound(nullptr, sfx_pistol);

        if (im.cnt_secret[0] >= (im.plrs[im.me].ssecret * 100) / im.wbs->maxsecret)
        {
            im.cnt_secret[0] = (im.plrs[im.me].ssecret * 100) / im.wbs->maxsecret;
            startSound(nullptr, sfx_barexp);
            im.sp_state++;
        }
    }

    else if (im.sp_state == 8)
    {
        if (!(im.bcnt & 3))
            startSound(nullptr, sfx_pistol);

        im.cnt_time += 3;

        if (im.cnt_time >= im.plrs[im.me].stime / TICRATE)
            im.cnt_time = im.plrs[im.me].stime / TICRATE;

        im.cnt_par += 3;

        if (im.cnt_par >= im.wbs->partime / TICRATE)
        {
            im.cnt_par = im.wbs->partime / TICRATE;

            if (im.cnt_time >= im.plrs[im.me].stime / TICRATE)
            {
                startSound(nullptr, sfx_barexp);
                im.sp_state++;
            }
        }
    }
    else if (im.sp_state == 10)
    {
        if (im.acceleratestage)
        {
            startSound(nullptr, sfx_sgcock);

            if (gameVersion().gamemode == commercial)
                initNoState();
            else
                initShowNextLoc();
        }
    }
    else if (im.sp_state & 1)
    {
        if (!--im.cnt_pause)
        {
            im.sp_state++;
            im.cnt_pause = TICRATE;
        }
    }
}

void drawStats()
{
    auto& im = intermissionState();

    // line height

    int lh = (3 * littleEndian(im.num[0]->height)) / 2;

    slamBackground();

    // draw animated background
    drawAnimatedBack();

    drawLF();

    drawPatch(SP_STATSX, SP_STATSY, FB, im.kills);
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY, im.cnt_kills[0]);

    drawPatch(SP_STATSX, SP_STATSY + lh, FB, im.items);
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + lh, im.cnt_items[0]);

    drawPatch(SP_STATSX, SP_STATSY + 2 * lh, FB, im.sp_secret);
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + 2 * lh, im.cnt_secret[0]);

    drawPatch(SP_TIMEX, SP_TIMEY, FB, im.time_patch);
    drawTime(SCREENWIDTH / 2 - SP_TIMEX, SP_TIMEY, im.cnt_time);

    if (im.wbs->epsd < 3)
    {
        drawPatch(SCREENWIDTH / 2 + SP_TIMEX, SP_TIMEY, FB, im.par);
        drawTime(SCREENWIDTH - SP_TIMEX, SP_TIMEY, im.cnt_par);
    }
}

void checkForAccelerate()
{
    auto& im = intermissionState();
    auto& players_ = playerState();

    int i;
    Player* player;

    // check for button presses to skip delays
    for (i = 0, player = players_.players.data(); i < MAXPLAYERS; i++, player++)
    {
        if (players_.playeringame[i])
        {
            if (player->cmd.buttons & BT_ATTACK)
            {
                if (!player->attackdown)
                    im.acceleratestage = 1;
                player->attackdown = true;
            }
            else
                player->attackdown = false;
            if (player->cmd.buttons & BT_USE)
            {
                if (!player->usedown)
                    im.acceleratestage = 1;
                player->usedown = true;
            }
            else
                player->usedown = false;
        }
    }
}

// Updates stuff each tick
void intermissionTicker()
{
    auto& im = intermissionState();
    const auto& session = gameSession();

    // counter for general background animation
    im.bcnt++;

    if (im.bcnt == 1)
    {
        // intermission music
        if (gameVersion().gamemode == commercial)
            changeMusic(mus_dm2int, true);
        else
            changeMusic(mus_inter, true);
    }

    checkForAccelerate();

    switch (im.state)
    {
        case StatCount:
            if (session.deathmatch)
                updateDeathmatchStats();
            else if (session.netgame)
                updateNetgameStats();
            else
                updateStats();
            break;

        case ShowNextLoc:
            updateShowNextLoc();
            break;

        case NoState:
            updateNoState();
            break;
    }
}

void loadIntermissionData()
{
    auto& im = intermissionState();
    const auto& session = gameSession();
    const auto& version = gameVersion();

    auto name = std::string {};

    if (version.gamemode == commercial)
        name = "INTERPIC";
    else
        name = concat("WIMAP", im.wbs->epsd);

    if (version.gamemode == retail)
    {
        if (im.wbs->epsd == 3)
            name = "INTERPIC";
    }

    // background
    im.bg = static_cast<Patch*>(cacheLumpName(name));
    drawPatch(0, 0, 1, im.bg);

    if (version.gamemode == commercial)
    {
        im.NUMCMAPS = 32;
        im.lnames.resize(im.NUMCMAPS);
        for (int i = 0; i < im.NUMCMAPS; i++)
        {
            name = "CWILV";
            if (i < 10)
                name += "0";
            name += std::to_string(i);
            im.lnames[i] = static_cast<Patch*>(cacheLumpName(name));
        }
    }
    else
    {
        im.lnames.resize(NUMMAPS);
        for (int i = 0; i < NUMMAPS; i++)
        {
            im.lnames[i] =
                static_cast<Patch*>(cacheLumpName(concat("WILV", im.wbs->epsd, i)));
        }

        // you are here
        im.yah[0] = static_cast<Patch*>(cacheLumpName("WIURH0"));

        // you are here (alt.)
        im.yah[1] = static_cast<Patch*>(cacheLumpName("WIURH1"));

        // splat
        im.splat = static_cast<Patch*>(cacheLumpName("WISPLAT"));

        if (im.wbs->epsd < 3)
        {
            for (int j = 0; j < NUMANIMS[im.wbs->epsd]; j++)
            {
                anim_t_wi_stuff* a = &anims_wi_stuff[im.wbs->epsd][j];
                for (int i = 0; i < a->nanims; i++)
                {
                    // MONDO HACK!
                    if (im.wbs->epsd != 1 || j != 8)
                    {
                        // animations
                        name = concat("WIA", im.wbs->epsd);
                        if (j < 10)
                            name += "0";
                        name += std::to_string(j);
                        if (i < 10)
                            name += "0";
                        name += std::to_string(i);
                        a->p[i] = static_cast<Patch*>(cacheLumpName(name));
                    }
                    else
                    {
                        // HACK ALERT!
                        a->p[i] = anims_wi_stuff[1][4].p[i];
                    }
                }
            }
        }
    }

    // More hacks on minus sign.
    im.wiminus = static_cast<Patch*>(cacheLumpName("WIMINUS"));

    for (int i = 0; i < 10; i++)
    {
        // numbers 0-9
        im.num[i] = static_cast<Patch*>(cacheLumpName(concat("WINUM", i)));
    }

    // percent sign
    im.percent = static_cast<Patch*>(cacheLumpName("WIPCNT"));

    // "finished"
    im.finished = static_cast<Patch*>(cacheLumpName("WIF"));

    // "entering"
    im.entering = static_cast<Patch*>(cacheLumpName("WIENTER"));

    // "kills"
    im.kills = static_cast<Patch*>(cacheLumpName("WIOSTK"));

    // "scrt"
    im.secret = static_cast<Patch*>(cacheLumpName("WIOSTS"));

    // "secret"
    im.sp_secret = static_cast<Patch*>(cacheLumpName("WISCRT2"));

    // Yuck.
    if (french)
    {
        // "items"
        if (session.netgame && !session.deathmatch)
            im.items = static_cast<Patch*>(cacheLumpName("WIOBJ"));
        else
            im.items = static_cast<Patch*>(cacheLumpName("WIOSTI"));
    }
    else
        im.items = static_cast<Patch*>(cacheLumpName("WIOSTI"));

    // "frgs"
    im.frags = static_cast<Patch*>(cacheLumpName("WIFRGS"));

    // ":"
    im.colon = static_cast<Patch*>(cacheLumpName("WICOLON"));

    // "time"
    im.time_patch = static_cast<Patch*>(cacheLumpName("WITIME"));

    // "sucks"
    im.sucks = static_cast<Patch*>(cacheLumpName("WISUCKS"));

    // "par"
    im.par = static_cast<Patch*>(cacheLumpName("WIPAR"));

    // "killers" (vertical)
    im.killers = static_cast<Patch*>(cacheLumpName("WIKILRS"));

    // "victims" (horiz)
    im.victims = static_cast<Patch*>(cacheLumpName("WIVCTMS"));

    // "total"
    im.total = static_cast<Patch*>(cacheLumpName("WIMSTT"));

    // your face
    im.star = static_cast<Patch*>(cacheLumpName("STFST01"));

    // dead face
    im.bstar = static_cast<Patch*>(cacheLumpName("STFDEAD0"));

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        // "1,2,3,4"
        im.p[i] = static_cast<Patch*>(cacheLumpName(concat("STPB", i)));

        // "1,2,3,4"
        im.bp[i] = static_cast<Patch*>(cacheLumpName(concat("WIBP", i + 1)));
    }
}

void unloadIntermissionData()
{
    // The patches are all lumps, and Doom::WadFile owns those now (Wad/WadFile.h):
    // they are permanent, and there is nothing to hand back. This whole function
    // used to be Z_ChangeTag(..., PU_CACHE) twenty-five times over, which said
    // "purge these if you need the space".
    //
    // lnames - the pointer array, the one thing here the intermission does own
    // (RAII-owned, Step 9) - must NOT be cleared here, and this function's call
    // site is why: updateNoState runs endIntermission and worldDone on the
    // intermission's last tic, but gamestate stays GS_INTERMISSION until the
    // *next* tic's doWorldDone, so displayFrame draws one more intermission
    // frame after this ran and drawEL reads lnames[wbs->next]. Vanilla had the
    // identical order and survived it because PU_CACHE left the memory readable;
    // an actual clear() turned that last draw into a read past the vector's size
    // (found by ASAN on the first level transition;
    // Tests/Sim/IntermissionTests.cpp pins it now). The next
    // loadIntermissionData resizes and refills the vector, so nothing is leaked
    // or stale by leaving it be - the Engine's destructor is its owner.
}

void drawIntermission()
{
    const auto& session = gameSession();

    switch (intermissionState().state)
    {
        case StatCount:
            if (session.deathmatch)
                drawDeathmatchStats();
            else if (session.netgame)
                drawNetgameStats();
            else
                drawStats();
            break;

        case ShowNextLoc:
            drawShowNextLoc();
            break;

        case NoState:
            drawNoState();
            break;
    }
}

void initIntermissionVariables(IntermissionStart* wbstartstruct)
{
    auto& im = intermissionState();
    const auto& version = gameVersion();

    im.wbs = wbstartstruct;

#ifdef RANGECHECKING
    if (version.gamemode != commercial)
    {
        if (version.gamemode == retail)
            RNGCHECK(im.wbs->epsd, 0, 3);
        else
            RNGCHECK(im.wbs->epsd, 0, 2);
    }
    else
    {
        RNGCHECK(im.wbs->last, 0, 8);
        RNGCHECK(im.wbs->next, 0, 8);
    }
    RNGCHECK(im.wbs->pnum, 0, MAXPLAYERS);
    RNGCHECK(im.wbs->pnum, 0, MAXPLAYERS);
#endif

    im.acceleratestage = 0;
    im.cnt = im.bcnt = 0;
    im.me = im.wbs->pnum;
    im.plrs = im.wbs->plyr;

    if (!im.wbs->maxkills)
        im.wbs->maxkills = 1;

    if (!im.wbs->maxitems)
        im.wbs->maxitems = 1;

    if (!im.wbs->maxsecret)
        im.wbs->maxsecret = 1;

    if (version.gamemode != retail)
        if (im.wbs->epsd > 2)
            im.wbs->epsd -= 3;
}

void startIntermission(IntermissionStart* wbstartstruct)
{
    const auto& session = gameSession();

    initIntermissionVariables(wbstartstruct);
    loadIntermissionData();

    if (session.deathmatch)
        initDeathmatchStats();
    else if (session.netgame)
        initNetgameStats();
    else
        initStats();
}

} // namespace Doom
