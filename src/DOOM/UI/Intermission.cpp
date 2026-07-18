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

#include "../doom_config.h"

#include "../doomstat.h"
#include "../m_random.h"
#include "../m_swap.h"
#include "../r_local.h"
#include "../sounds.h" // Data.
#include "../v_video.h" // Needs access to LFB.
#include "../Wad/WadFile.h"
#include "../wi_stuff.h"

#include "Intermission.h"
#include "IntermissionState.h"

#include "../Render/Video.h"
#include <ea_data_structures/Structures/Array.h>

#include "../Game/Game.h"
#include "../Game/GameSession.h"
#include "../Game/GameVersion.h"
#include "../Game/PlayerState.h"
#include "../Game/Sound.h"
#include "../Sim/Random.h"
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
#define NUMEPISODES 4
#define NUMMAPS 9

// GLOBAL LOCATIONS
#define WI_TITLEY 2
#define WI_SPACINGY 33

// SINGPLE-PLAYER STUFF
#define SP_STATSX 50
#define SP_STATSY 50

#define SP_TIMEX 16
#define SP_TIMEY (SCREENHEIGHT - 32)

// NET GAME STUFF
#define NG_STATSY 50
#define NG_STATSX (32 + SHORT(star->width) / 2 + 32 * !dofrags)

#define NG_SPACINGX 64

// DEATHMATCH STUFF
#define DM_MATRIXX 42
#define DM_MATRIXY 68

#define DM_SPACINGX 40

#define DM_TOTALSX 269

#define DM_KILLERSX 10
#define DM_KILLERSY 100
#define DM_VICTIMSX 5
#define DM_VICTIMSY 50

#define FB 0

// States for single-player
#define SP_KILLS 0
#define SP_ITEMS 2
#define SP_SECRET 4
#define SP_FRAGS 6
#define SP_TIME 8
#define SP_PAR ST_TIME

#define SP_PAUSE 1

// in seconds
#define SHOWNEXTLOCDELAY 4
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

static EA::Array<EA::Array<Point, NUMMAPS>, NUMEPISODES> lnodes = {
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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
static EA::Array<anim_t_wi_stuff, 10> epsd0animinfo = {
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

static EA::Array<anim_t_wi_stuff, 9> epsd1animinfo = {
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 1},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 2},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 3},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 4},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 5},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 6},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 7},
    {ANIM_LEVEL, TICRATE / 3, 3, {192, 144}, 8},
    {ANIM_LEVEL, TICRATE / 3, 1, {128, 136}, 8}};

static EA::Array<anim_t_wi_stuff, 6> epsd2animinfo = {
    {ANIM_ALWAYS, TICRATE / 3, 3, {104, 168}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {40, 136}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {160, 96}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {104, 80}},
    {ANIM_ALWAYS, TICRATE / 3, 3, {120, 32}},
    {ANIM_ALWAYS, TICRATE / 4, 3, {40, 0}}};
#pragma GCC diagnostic pop

static EA::Array<int, NUMEPISODES> NUMANIMS = {
    epsd0animinfo.size(), epsd1animinfo.size(), epsd2animinfo.size()};

static EA::Array<anim_t_wi_stuff*, NUMEPISODES> anims_wi_stuff = {
    epsd0animinfo.data(), epsd1animinfo.data(), epsd2animinfo.data()};

//
// GENERAL DATA
//

// The intermission's residual runtime state and loaded graphics now live on the Engine
// (UI/IntermissionState.h, moved by the file-scope-statics sweep - REFACTOR.md, Step 5). The
// vanilla names below are references onto that member, so every use is unchanged. The
// animation/layout data tables above (lnodes / epsd*animinfo / NUMANIMS / anims_wi_stuff) stay
// file-local: anims_wi_stuff points into epsd*animinfo, a self-referential-pointer table that does
// not survive being a copyable struct member (the trap AutomapView's cheat sequence documented).

//
// Locally used stuff.
//

// used to accelerate or skip a stage
static int& acceleratestage = intermissionState().acceleratestage;

// wbs->pnum
static int& me = intermissionState().me;

// specifies current state
static IntermissionPhase& state = intermissionState().state;

// contains information passed into intermission
static IntermissionStart*& wbs = intermissionState().wbs;

static IntermissionPlayer*& plrs = intermissionState().plrs; // wbs->plyr[]

// used for general timing
static int& cnt = intermissionState().cnt;

// used for timing of background animation
static int& bcnt = intermissionState().bcnt;

// signals to refresh everything for one frame
static int& firstrefresh = intermissionState().firstrefresh;

static int (&cnt_kills)[MAXPLAYERS] = intermissionState().cnt_kills;
static int (&cnt_items)[MAXPLAYERS] = intermissionState().cnt_items;
static int (&cnt_secret)[MAXPLAYERS] = intermissionState().cnt_secret;
static int& cnt_time = intermissionState().cnt_time;
static int& cnt_par = intermissionState().cnt_par;
static int& cnt_pause = intermissionState().cnt_pause;

// # of commercial levels
static int& NUMCMAPS = intermissionState().NUMCMAPS;

//
// GRAPHICS
//

// background (map of levels).
static Patch*& bg = intermissionState().bg;

// You Are Here graphic
static Patch* (&yah)[2] = intermissionState().yah;

// splat
static Patch*& splat = intermissionState().splat;

// %, : graphics
static Patch*& percent = intermissionState().percent;
static Patch*& colon = intermissionState().colon;

// 0-9 graphic
static Patch* (&num)[10] = intermissionState().num;

// minus sign
static Patch*& wiminus = intermissionState().wiminus;

// "Finished!" graphics
static Patch*& finished = intermissionState().finished;

// "Entering" graphic
static Patch*& entering = intermissionState().entering;

// "secret"
static Patch*& sp_secret = intermissionState().sp_secret;

// "Kills", "Scrt", "Items", "Frags"
static Patch*& kills = intermissionState().kills;
static Patch*& secret = intermissionState().secret;
static Patch*& items = intermissionState().items;
static Patch*& frags = intermissionState().frags;

// Time sucks.
static Patch*& time_patch = intermissionState().time_patch;
static Patch*& par = intermissionState().par;
static Patch*& sucks = intermissionState().sucks;

// "killers", "victims"
static Patch*& killers = intermissionState().killers;
static Patch*& victims = intermissionState().victims;

// "Total", your face, your dead face
static Patch*& total = intermissionState().total;
static Patch*& star = intermissionState().star;
static Patch*& bstar = intermissionState().bstar;

// "red P[1..MAXPLAYERS]"
static Patch* (&p)[MAXPLAYERS] = intermissionState().p;

// "gray P[1..MAXPLAYERS]"
static Patch* (&bp)[MAXPLAYERS] = intermissionState().bp;

// Name graphics of each level (centered)
static Patch**& lnames = intermissionState().lnames;

static doom_boolean& snl_pointeron = intermissionState().snl_pointeron;
static int& dm_state = intermissionState().dm_state;
static int (&dm_frags)[MAXPLAYERS][MAXPLAYERS] = intermissionState().dm_frags;
static int (&dm_totals)[MAXPLAYERS] = intermissionState().dm_totals;
static int (&cnt_frags)[MAXPLAYERS] = intermissionState().cnt_frags;
static int& dofrags = intermissionState().dofrags;
static int& ng_state = intermissionState().ng_state;
static int& sp_state = intermissionState().sp_state;

//
// CODE
//

void slamBackground()
{
    doom_memcpy(screens[0], screens[1], SCREENWIDTH * SCREENHEIGHT);
    Doom::markRect(0, 0, SCREENWIDTH, SCREENHEIGHT);
}

// The ticker is used to detect keys
//  because of timing issues in netgames.
doom_boolean intermissionResponder(Event*)
{
    return false;
}

// Draws "<Levelname> Finished!"
void drawLF()
{
    int y = WI_TITLEY;

    // draw <LevelName>
    Doom::drawPatch((SCREENWIDTH - SHORT(lnames[wbs->last]->width)) / 2,
                    y,
                    FB,
                    lnames[wbs->last]);

    // draw "Finished!"
    y += (5 * SHORT(lnames[wbs->last]->height)) / 4;

    Doom::drawPatch((SCREENWIDTH - SHORT(finished->width)) / 2, y, FB, finished);
}

// Draws "Entering <LevelName>"
void drawEL()
{
    int y = WI_TITLEY;

    // draw "Entering"
    Doom::drawPatch((SCREENWIDTH - SHORT(entering->width)) / 2, y, FB, entering);

    // draw level
    y += (5 * SHORT(lnames[wbs->next]->height)) / 4;

    Doom::drawPatch((SCREENWIDTH - SHORT(lnames[wbs->next]->width)) / 2,
                    y,
                    FB,
                    lnames[wbs->next]);
}

void drawOnLnode(int n, Patch* c[])
{
    int i;
    int left;
    int top;
    int right;
    int bottom;
    doom_boolean fits = false;

    i = 0;
    do
    {
        left = lnodes[wbs->epsd][n].x - SHORT(c[i]->leftoffset);
        top = lnodes[wbs->epsd][n].y - SHORT(c[i]->topoffset);
        right = left + SHORT(c[i]->width);
        bottom = top + SHORT(c[i]->height);

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
        Doom::drawPatch(lnodes[wbs->epsd][n].x, lnodes[wbs->epsd][n].y, FB, c[i]);
    }
    else
    {
        // DEBUG
        //doom_print("Could not place patch on level %d", n + 1);
        doom_print("Could not place patch on level ");
        doom_print(doom_itoa(n + 1, 10));
    }
}

void initAnimatedBack()
{
    anim_t_wi_stuff* a;

    if (gameVersion().gamemode == commercial)
        return;

    if (wbs->epsd > 2)
        return;

    for (int i = 0; i < NUMANIMS[wbs->epsd]; i++)
    {
        a = &anims_wi_stuff[wbs->epsd][i];

        // init variables
        a->ctr = -1;

        // specify the next time to draw it
        if (a->type == ANIM_ALWAYS)
            a->nexttic = bcnt + 1 + (Doom::randomness().forMenu() % a->period);
        else if (a->type == ANIM_RANDOM)
            a->nexttic = bcnt + 1 + a->data2 + (Doom::randomness().forMenu() % a->data1);
        else if (a->type == ANIM_LEVEL)
            a->nexttic = bcnt + 1;
    }
}

void updateAnimatedBack()
{
    anim_t_wi_stuff* a;

    if (gameVersion().gamemode == commercial)
        return;

    if (wbs->epsd > 2)
        return;

    for (int i = 0; i < NUMANIMS[wbs->epsd]; i++)
    {
        a = &anims_wi_stuff[wbs->epsd][i];

        if (bcnt == a->nexttic)
        {
            switch (a->type)
            {
                case ANIM_ALWAYS:
                    if (++a->ctr >= a->nanims)
                        a->ctr = 0;
                    a->nexttic = bcnt + a->period;
                    break;

                case ANIM_RANDOM:
                    a->ctr++;
                    if (a->ctr == a->nanims)
                    {
                        a->ctr = -1;
                        a->nexttic = bcnt + a->data2 + (Doom::randomness().forMenu() % a->data1);
                    }
                    else
                        a->nexttic = bcnt + a->period;
                    break;

                case ANIM_LEVEL:
                    // gawd-awful hack for level anims
                    if (!(state == StatCount && i == 7) && wbs->next == a->data1)
                    {
                        a->ctr++;
                        if (a->ctr == a->nanims)
                            a->ctr--;
                        a->nexttic = bcnt + a->period;
                    }
                    break;
            }
        }
    }
}

void drawAnimatedBack()
{
    anim_t_wi_stuff* a;

    if (commercial)
        return;

    if (wbs->epsd > 2)
        return;

    for (int i = 0; i < NUMANIMS[wbs->epsd]; i++)
    {
        a = &anims_wi_stuff[wbs->epsd][i];

        if (a->ctr >= 0)
            Doom::drawPatch(a->loc.x, a->loc.y, FB, a->p[a->ctr]);
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
    int fontwidth = SHORT(num[0]->width);
    int neg;
    int temp;

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
            temp = n;

            while (temp)
            {
                temp /= 10;
                digits++;
            }
        }
    }

    neg = n < 0;
    if (neg)
        n = -n;

    // if non-number, do not draw it
    if (n == 1994)
        return 0;

    // draw the new number
    while (digits--)
    {
        x -= fontwidth;
        Doom::drawPatch(x, y, FB, num[n % 10]);
        n /= 10;
    }

    // draw a minus sign if necessary
    if (neg)
        Doom::drawPatch(x -= 8, y, FB, wiminus);

    return x;
}

void drawPercent(int x, int y, int p)
{
    if (p < 0)
        return;

    Doom::drawPatch(x, y, FB, percent);
    drawIntermissionNum(x, y, p, -1);
}

//
// Display level completion time and par,
//  or "sucks" message if overflow.
//
void drawTime(int x, int y, int t)
{
    int div;
    int n;

    if (t < 0)
        return;

    if (t <= 61 * 59)
    {
        div = 1;

        do
        {
            n = (t / div) % 60;
            x = drawIntermissionNum(x, y, n, 2) - SHORT(colon->width);
            div *= 60;

            // draw
            if (div == 60 || t / div)
                Doom::drawPatch(x, y, FB, colon);

        } while (t / div);
    }
    else
    {
        // "sucks"
        Doom::drawPatch(x - SHORT(sucks->width), y, FB, sucks);
    }
}

void endIntermission()
{
    void unloadIntermissionData();
    unloadIntermissionData();
}

void initNoState()
{
    state = NoState;
    acceleratestage = 0;
    cnt = 10;
}

void updateNoState()
{
    updateAnimatedBack();

    if (!--cnt)
    {
        endIntermission();
        Doom::worldDone();
    }
}

void initShowNextLoc()
{
    state = ShowNextLoc;
    acceleratestage = 0;
    cnt = SHOWNEXTLOCDELAY * TICRATE;

    initAnimatedBack();
}

void updateShowNextLoc()
{
    updateAnimatedBack();

    if (!--cnt || acceleratestage)
        initNoState();
    else
        snl_pointeron = (cnt & 31) < 20;
}

void drawShowNextLoc()
{
    const auto& version = gameVersion();

    int last;

    slamBackground();

    // draw animated background
    drawAnimatedBack();

    if (version.gamemode != commercial)
    {
        if (wbs->epsd > 2)
        {
            drawEL();
            return;
        }

        last = (wbs->last == 8) ? wbs->next - 1 : wbs->last;

        // draw a splat on taken cities.
        for (int i = 0; i <= last; i++)
            drawOnLnode(i, &splat);

        // splat the secret level?
        if (wbs->didsecret)
            drawOnLnode(8, &splat);

        // draw flashing ptr
        if (snl_pointeron)
            drawOnLnode(wbs->next, yah);
    }

    // draws which level you are entering..
    if ((version.gamemode != commercial) || wbs->next != 30)
        drawEL();
}

void drawNoState()
{
    snl_pointeron = true;
    drawShowNextLoc();
}

int fragSum(int playernum)
{
    int frags = 0;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (playerState().playeringame[i] && i != playernum)
        {
            frags += plrs[playernum].frags[i];
        }
    }

    // JDC hack - negative frags.
    frags -= plrs[playernum].frags[playernum];
    // UNUSED if (frags < 0)
    //         frags = 0;

    return frags;
}

void initDeathmatchStats()
{
    const auto& players_ = playerState();

    state = StatCount;
    acceleratestage = 0;
    dm_state = 1;

    cnt_pause = TICRATE;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (players_.playeringame[i])
        {
            for (int j = 0; j < MAXPLAYERS; j++)
                if (players_.playeringame[j])
                    dm_frags[i][j] = 0;

            dm_totals[i] = 0;
        }
    }

    initAnimatedBack();
}

void updateDeathmatchStats()
{
    const auto& players_ = playerState();

    doom_boolean stillticking;

    updateAnimatedBack();

    if (acceleratestage && dm_state != 4)
    {
        acceleratestage = 0;

        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (players_.playeringame[i])
            {
                for (int j = 0; j < MAXPLAYERS; j++)
                    if (players_.playeringame[j])
                        dm_frags[i][j] = plrs[i].frags[j];

                dm_totals[i] = fragSum(i);
            }
        }

        Doom::startSound(0, sfx_barexp);
        dm_state = 4;
    }

    if (dm_state == 2)
    {
        if (!(bcnt & 3))
            Doom::startSound(0, sfx_pistol);

        stillticking = false;

        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (players_.playeringame[i])
            {
                for (int j = 0; j < MAXPLAYERS; j++)
                {
                    if (players_.playeringame[j]
                        && dm_frags[i][j] != plrs[i].frags[j])
                    {
                        if (plrs[i].frags[j] < 0)
                            dm_frags[i][j]--;
                        else
                            dm_frags[i][j]++;

                        if (dm_frags[i][j] > 99)
                            dm_frags[i][j] = 99;

                        if (dm_frags[i][j] < -99)
                            dm_frags[i][j] = -99;

                        stillticking = true;
                    }
                }
                dm_totals[i] = fragSum(i);

                if (dm_totals[i] > 99)
                    dm_totals[i] = 99;

                if (dm_totals[i] < -99)
                    dm_totals[i] = -99;
            }
        }
        if (!stillticking)
        {
            Doom::startSound(0, sfx_barexp);
            dm_state++;
        }
    }
    else if (dm_state == 4)
    {
        if (acceleratestage)
        {
            Doom::startSound(0, sfx_slop);

            if (gameVersion().gamemode == commercial)
                initNoState();
            else
                initShowNextLoc();
        }
    }
    else if (dm_state & 1)
    {
        if (!--cnt_pause)
        {
            dm_state++;
            cnt_pause = TICRATE;
        }
    }
}

void drawDeathmatchStats()
{
    const auto& players_ = playerState();

    int x;
    int y;
    int w;

    slamBackground();

    // draw animated background
    drawAnimatedBack();
    drawLF();

    // draw stat titles (top line)
    Doom::drawPatch(DM_TOTALSX - SHORT(total->width) / 2,
                    DM_MATRIXY - WI_SPACINGY + 10,
                    FB,
                    total);

    Doom::drawPatch(DM_KILLERSX, DM_KILLERSY, FB, killers);
    Doom::drawPatch(DM_VICTIMSX, DM_VICTIMSY, FB, victims);

    // draw P?
    x = DM_MATRIXX + DM_SPACINGX;
    y = DM_MATRIXY;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (players_.playeringame[i])
        {
            Doom::drawPatch(
                x - SHORT(p[i]->width) / 2, DM_MATRIXY - WI_SPACINGY, FB, p[i]);

            Doom::drawPatch(DM_MATRIXX - SHORT(p[i]->width) / 2, y, FB, p[i]);

            if (i == me)
            {
                Doom::drawPatch(
                    x - SHORT(p[i]->width) / 2, DM_MATRIXY - WI_SPACINGY, FB, bstar);

                Doom::drawPatch(DM_MATRIXX - SHORT(p[i]->width) / 2, y, FB, star);
            }
        }
        else
        {
            // Doom::drawPatch(x-SHORT(bp[i]->width)/2,
            //   DM_MATRIXY - WI_SPACINGY, FB, bp[i]);
            // Doom::drawPatch(DM_MATRIXX-SHORT(bp[i]->width)/2,
            //   y, FB, bp[i]);
        }
        x += DM_SPACINGX;
        y += WI_SPACINGY;
    }

    // draw stats
    y = DM_MATRIXY + 10;
    w = SHORT(num[0]->width);

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        x = DM_MATRIXX + DM_SPACINGX;

        if (players_.playeringame[i])
        {
            for (int j = 0; j < MAXPLAYERS; j++)
            {
                if (players_.playeringame[j])
                    drawIntermissionNum(x + w, y, dm_frags[i][j], 2);

                x += DM_SPACINGX;
            }
            drawIntermissionNum(DM_TOTALSX + w, y, dm_totals[i], 2);
        }
        y += WI_SPACINGY;
    }
}

void initNetgameStats()
{
    state = StatCount;
    acceleratestage = 0;
    ng_state = 1;

    cnt_pause = TICRATE;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (!playerState().playeringame[i])
            continue;

        cnt_kills[i] = cnt_items[i] = cnt_secret[i] = cnt_frags[i] = 0;

        dofrags += fragSum(i);
    }

    dofrags = !!dofrags;

    initAnimatedBack();
}

void updateNetgameStats()
{
    const auto& players_ = playerState();

    int fsum;

    doom_boolean stillticking;

    updateAnimatedBack();

    if (acceleratestage && ng_state != 10)
    {
        acceleratestage = 0;

        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (!players_.playeringame[i])
                continue;

            cnt_kills[i] = (plrs[i].skills * 100) / wbs->maxkills;
            cnt_items[i] = (plrs[i].sitems * 100) / wbs->maxitems;
            cnt_secret[i] = (plrs[i].ssecret * 100) / wbs->maxsecret;

            if (dofrags)
                cnt_frags[i] = fragSum(i);
        }
        Doom::startSound(0, sfx_barexp);
        ng_state = 10;
    }

    if (ng_state == 2)
    {
        if (!(bcnt & 3))
            Doom::startSound(0, sfx_pistol);

        stillticking = false;

        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (!players_.playeringame[i])
                continue;

            cnt_kills[i] += 2;

            if (cnt_kills[i] >= (plrs[i].skills * 100) / wbs->maxkills)
                cnt_kills[i] = (plrs[i].skills * 100) / wbs->maxkills;
            else
                stillticking = true;
        }

        if (!stillticking)
        {
            Doom::startSound(0, sfx_barexp);
            ng_state++;
        }
    }
    else if (ng_state == 4)
    {
        if (!(bcnt & 3))
            Doom::startSound(0, sfx_pistol);

        stillticking = false;

        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (!players_.playeringame[i])
                continue;

            cnt_items[i] += 2;
            if (cnt_items[i] >= (plrs[i].sitems * 100) / wbs->maxitems)
                cnt_items[i] = (plrs[i].sitems * 100) / wbs->maxitems;
            else
                stillticking = true;
        }
        if (!stillticking)
        {
            Doom::startSound(0, sfx_barexp);
            ng_state++;
        }
    }
    else if (ng_state == 6)
    {
        if (!(bcnt & 3))
            Doom::startSound(0, sfx_pistol);

        stillticking = false;

        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (!players_.playeringame[i])
                continue;

            cnt_secret[i] += 2;

            if (cnt_secret[i] >= (plrs[i].ssecret * 100) / wbs->maxsecret)
                cnt_secret[i] = (plrs[i].ssecret * 100) / wbs->maxsecret;
            else
                stillticking = true;
        }

        if (!stillticking)
        {
            Doom::startSound(0, sfx_barexp);
            ng_state += 1 + 2 * !dofrags;
        }
    }
    else if (ng_state == 8)
    {
        if (!(bcnt & 3))
            Doom::startSound(0, sfx_pistol);

        stillticking = false;

        for (int i = 0; i < MAXPLAYERS; i++)
        {
            if (!players_.playeringame[i])
                continue;

            cnt_frags[i] += 1;

            if (cnt_frags[i] >= (fsum = fragSum(i)))
                cnt_frags[i] = fsum;
            else
                stillticking = true;
        }

        if (!stillticking)
        {
            Doom::startSound(0, sfx_pldeth);
            ng_state++;
        }
    }
    else if (ng_state == 10)
    {
        if (acceleratestage)
        {
            Doom::startSound(0, sfx_sgcock);
            if (gameVersion().gamemode == commercial)
                initNoState();
            else
                initShowNextLoc();
        }
    }
    else if (ng_state & 1)
    {
        if (!--cnt_pause)
        {
            ng_state++;
            cnt_pause = TICRATE;
        }
    }
}

void drawNetgameStats()
{
    int x;
    int y;
    int pwidth = SHORT(percent->width);

    slamBackground();

    // draw animated background
    drawAnimatedBack();

    drawLF();

    // draw stat titles (top line)
    Doom::drawPatch(
        NG_STATSX + NG_SPACINGX - SHORT(kills->width), NG_STATSY, FB, kills);

    Doom::drawPatch(
        NG_STATSX + 2 * NG_SPACINGX - SHORT(items->width), NG_STATSY, FB, items);

    Doom::drawPatch(
        NG_STATSX + 3 * NG_SPACINGX - SHORT(secret->width), NG_STATSY, FB, secret);

    if (dofrags)
        Doom::drawPatch(
            NG_STATSX + 4 * NG_SPACINGX - SHORT(frags->width), NG_STATSY, FB, frags);

    // draw stats
    y = NG_STATSY + SHORT(kills->height);

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        if (!playerState().playeringame[i])
            continue;

        x = NG_STATSX;
        Doom::drawPatch(x - SHORT(p[i]->width), y, FB, p[i]);

        if (i == me)
            Doom::drawPatch(x - SHORT(p[i]->width), y, FB, star);

        x += NG_SPACINGX;
        drawPercent(x - pwidth, y + 10, cnt_kills[i]);
        x += NG_SPACINGX;
        drawPercent(x - pwidth, y + 10, cnt_items[i]);
        x += NG_SPACINGX;
        drawPercent(x - pwidth, y + 10, cnt_secret[i]);
        x += NG_SPACINGX;

        if (dofrags)
            drawIntermissionNum(x, y + 10, cnt_frags[i], -1);

        y += WI_SPACINGY;
    }
}

void initStats()
{
    state = StatCount;
    acceleratestage = 0;
    sp_state = 1;
    cnt_kills[0] = cnt_items[0] = cnt_secret[0] = -1;
    cnt_time = cnt_par = -1;
    cnt_pause = TICRATE;

    initAnimatedBack();
}

void updateStats()
{
    updateAnimatedBack();

    if (acceleratestage && sp_state != 10)
    {
        acceleratestage = 0;
        cnt_kills[0] = (plrs[me].skills * 100) / wbs->maxkills;
        cnt_items[0] = (plrs[me].sitems * 100) / wbs->maxitems;
        cnt_secret[0] = (plrs[me].ssecret * 100) / wbs->maxsecret;
        cnt_time = plrs[me].stime / TICRATE;
        cnt_par = wbs->partime / TICRATE;
        Doom::startSound(0, sfx_barexp);
        sp_state = 10;
    }

    if (sp_state == 2)
    {
        cnt_kills[0] += 2;

        if (!(bcnt & 3))
            Doom::startSound(0, sfx_pistol);

        if (cnt_kills[0] >= (plrs[me].skills * 100) / wbs->maxkills)
        {
            cnt_kills[0] = (plrs[me].skills * 100) / wbs->maxkills;
            Doom::startSound(0, sfx_barexp);
            sp_state++;
        }
    }
    else if (sp_state == 4)
    {
        cnt_items[0] += 2;

        if (!(bcnt & 3))
            Doom::startSound(0, sfx_pistol);

        if (cnt_items[0] >= (plrs[me].sitems * 100) / wbs->maxitems)
        {
            cnt_items[0] = (plrs[me].sitems * 100) / wbs->maxitems;
            Doom::startSound(0, sfx_barexp);
            sp_state++;
        }
    }
    else if (sp_state == 6)
    {
        cnt_secret[0] += 2;

        if (!(bcnt & 3))
            Doom::startSound(0, sfx_pistol);

        if (cnt_secret[0] >= (plrs[me].ssecret * 100) / wbs->maxsecret)
        {
            cnt_secret[0] = (plrs[me].ssecret * 100) / wbs->maxsecret;
            Doom::startSound(0, sfx_barexp);
            sp_state++;
        }
    }

    else if (sp_state == 8)
    {
        if (!(bcnt & 3))
            Doom::startSound(0, sfx_pistol);

        cnt_time += 3;

        if (cnt_time >= plrs[me].stime / TICRATE)
            cnt_time = plrs[me].stime / TICRATE;

        cnt_par += 3;

        if (cnt_par >= wbs->partime / TICRATE)
        {
            cnt_par = wbs->partime / TICRATE;

            if (cnt_time >= plrs[me].stime / TICRATE)
            {
                Doom::startSound(0, sfx_barexp);
                sp_state++;
            }
        }
    }
    else if (sp_state == 10)
    {
        if (acceleratestage)
        {
            Doom::startSound(0, sfx_sgcock);

            if (gameVersion().gamemode == commercial)
                initNoState();
            else
                initShowNextLoc();
        }
    }
    else if (sp_state & 1)
    {
        if (!--cnt_pause)
        {
            sp_state++;
            cnt_pause = TICRATE;
        }
    }
}

void drawStats()
{
    // line height
    int lh;

    lh = (3 * SHORT(num[0]->height)) / 2;

    slamBackground();

    // draw animated background
    drawAnimatedBack();

    drawLF();

    Doom::drawPatch(SP_STATSX, SP_STATSY, FB, kills);
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY, cnt_kills[0]);

    Doom::drawPatch(SP_STATSX, SP_STATSY + lh, FB, items);
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + lh, cnt_items[0]);

    Doom::drawPatch(SP_STATSX, SP_STATSY + 2 * lh, FB, sp_secret);
    drawPercent(SCREENWIDTH - SP_STATSX, SP_STATSY + 2 * lh, cnt_secret[0]);

    Doom::drawPatch(SP_TIMEX, SP_TIMEY, FB, time_patch);
    drawTime(SCREENWIDTH / 2 - SP_TIMEX, SP_TIMEY, cnt_time);

    if (wbs->epsd < 3)
    {
        Doom::drawPatch(SCREENWIDTH / 2 + SP_TIMEX, SP_TIMEY, FB, par);
        drawTime(SCREENWIDTH - SP_TIMEX, SP_TIMEY, cnt_par);
    }
}

void checkForAccelerate()
{
    auto& players_ = playerState();

    int i;
    Player* player;

    // check for button presses to skip delays
    for (i = 0, player = players_.players; i < MAXPLAYERS; i++, player++)
    {
        if (players_.playeringame[i])
        {
            if (player->cmd.buttons & BT_ATTACK)
            {
                if (!player->attackdown)
                    acceleratestage = 1;
                player->attackdown = true;
            }
            else
                player->attackdown = false;
            if (player->cmd.buttons & BT_USE)
            {
                if (!player->usedown)
                    acceleratestage = 1;
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
    const auto& session = gameSession();

    // counter for general background animation
    bcnt++;

    if (bcnt == 1)
    {
        // intermission music
        if (gameVersion().gamemode == commercial)
            Doom::changeMusic(mus_dm2int, true);
        else
            Doom::changeMusic(mus_inter, true);
    }

    checkForAccelerate();

    switch (state)
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
    const auto& session = gameSession();
    const auto& version = gameVersion();

    EA::Array<char, 9> name;
    anim_t_wi_stuff* a;

    if (version.gamemode == commercial)
        doom_strcpy(name.data(), "INTERPIC");
    else
    {
        //doom_sprintf(name, "WIMAP%d", wbs->epsd);
        doom_strcpy(name.data(), "WIMAP");
        doom_concat(name.data(), doom_itoa(wbs->epsd, 10));
    }

    if (version.gamemode == retail)
    {
        if (wbs->epsd == 3)
            doom_strcpy(name.data(), "INTERPIC");
    }

    // background
    bg = static_cast<Patch*>(Doom::cacheLumpName(name.data()));
    Doom::drawPatch(0, 0, 1, bg);

    if (version.gamemode == commercial)
    {
        NUMCMAPS = 32;
        lnames = static_cast<Patch**>(doom_malloc(sizeof(Patch*) * NUMCMAPS));
        for (int i = 0; i < NUMCMAPS; i++)
        {
            //doom_sprintf(name, "CWILV%2.2d", i);
            doom_strcpy(name.data(), "CWILV");
            if (i < 10)
                doom_concat(name.data(), "0");
            doom_concat(name.data(), doom_itoa(i, 10));
            lnames[i] = static_cast<Patch*>(Doom::cacheLumpName(name.data()));
        }
    }
    else
    {
        lnames = static_cast<Patch**>(doom_malloc(sizeof(Patch*) * NUMMAPS));
        for (int i = 0; i < NUMMAPS; i++)
        {
            //doom_sprintf(name, "WILV%d%d", wbs->epsd, i);
            doom_strcpy(name.data(), "WILV");
            doom_concat(name.data(), doom_itoa(wbs->epsd, 10));
            doom_concat(name.data(), doom_itoa(i, 10));
            lnames[i] = static_cast<Patch*>(Doom::cacheLumpName(name.data()));
        }

        // you are here
        yah[0] = static_cast<Patch*>(Doom::cacheLumpName("WIURH0"));

        // you are here (alt.)
        yah[1] = static_cast<Patch*>(Doom::cacheLumpName("WIURH1"));

        // splat
        splat = static_cast<Patch*>(Doom::cacheLumpName("WISPLAT"));

        if (wbs->epsd < 3)
        {
            for (int j = 0; j < NUMANIMS[wbs->epsd]; j++)
            {
                a = &anims_wi_stuff[wbs->epsd][j];
                for (int i = 0; i < a->nanims; i++)
                {
                    // MONDO HACK!
                    if (wbs->epsd != 1 || j != 8)
                    {
                        // animations
                        //doom_sprintf(name, "WIA%d%.2d%.2d", wbs->epsd, j, i);
                        doom_strcpy(name.data(), "WIA");
                        doom_concat(name.data(), doom_itoa(wbs->epsd, 10));
                        if (j < 10)
                            doom_concat(name.data(), "0");
                        doom_concat(name.data(), doom_itoa(j, 10));
                        if (i < 10)
                            doom_concat(name.data(), "0");
                        doom_concat(name.data(), doom_itoa(i, 10));
                        a->p[i] =
                            static_cast<Patch*>(Doom::cacheLumpName(name.data()));
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
    wiminus = static_cast<Patch*>(Doom::cacheLumpName("WIMINUS"));

    for (int i = 0; i < 10; i++)
    {
        // numbers 0-9
        //doom_sprintf(name, "WINUM%d", i);
        doom_strcpy(name.data(), "WINUM");
        doom_concat(name.data(), doom_itoa(i, 10));
        num[i] = static_cast<Patch*>(Doom::cacheLumpName(name.data()));
    }

    // percent sign
    percent = static_cast<Patch*>(Doom::cacheLumpName("WIPCNT"));

    // "finished"
    finished = static_cast<Patch*>(Doom::cacheLumpName("WIF"));

    // "entering"
    entering = static_cast<Patch*>(Doom::cacheLumpName("WIENTER"));

    // "kills"
    kills = static_cast<Patch*>(Doom::cacheLumpName("WIOSTK"));

    // "scrt"
    secret = static_cast<Patch*>(Doom::cacheLumpName("WIOSTS"));

    // "secret"
    sp_secret = static_cast<Patch*>(Doom::cacheLumpName("WISCRT2"));

    // Yuck.
    if (french)
    {
        // "items"
        if (session.netgame && !session.deathmatch)
            items = static_cast<Patch*>(Doom::cacheLumpName("WIOBJ"));
        else
            items = static_cast<Patch*>(Doom::cacheLumpName("WIOSTI"));
    }
    else
        items = static_cast<Patch*>(Doom::cacheLumpName("WIOSTI"));

    // "frgs"
    frags = static_cast<Patch*>(Doom::cacheLumpName("WIFRGS"));

    // ":"
    colon = static_cast<Patch*>(Doom::cacheLumpName("WICOLON"));

    // "time"
    time_patch = static_cast<Patch*>(Doom::cacheLumpName("WITIME"));

    // "sucks"
    sucks = static_cast<Patch*>(Doom::cacheLumpName("WISUCKS"));

    // "par"
    par = static_cast<Patch*>(Doom::cacheLumpName("WIPAR"));

    // "killers" (vertical)
    killers = static_cast<Patch*>(Doom::cacheLumpName("WIKILRS"));

    // "victims" (horiz)
    victims = static_cast<Patch*>(Doom::cacheLumpName("WIVCTMS"));

    // "total"
    total = static_cast<Patch*>(Doom::cacheLumpName("WIMSTT"));

    // your face
    star = static_cast<Patch*>(Doom::cacheLumpName("STFST01"));

    // dead face
    bstar = static_cast<Patch*>(Doom::cacheLumpName("STFDEAD0"));

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        // "1,2,3,4"
        //doom_sprintf(name, "STPB%d", i);
        doom_strcpy(name.data(), "STPB");
        doom_concat(name.data(), doom_itoa(i, 10));
        p[i] = static_cast<Patch*>(Doom::cacheLumpName(name.data()));

        // "1,2,3,4"
        //doom_sprintf(name, "WIBP%d", i + 1);
        doom_strcpy(name.data(), "WIBP");
        doom_concat(name.data(), doom_itoa(i + 1, 10));
        bp[i] = static_cast<Patch*>(Doom::cacheLumpName(name.data()));
    }
}

void unloadIntermissionData()
{
    // The patches are all lumps, and Doom::WadFile owns those now (Wad/WadFile.h):
    // they are permanent, and there is nothing to hand back. This whole function
    // used to be Z_ChangeTag(..., PU_CACHE) twenty-five times over, which said
    // "purge these if you need the space".
    //
    // lnames is not a lump. It is the array of pointers *to* the lumps, allocated
    // by loadIntermissionData, and it is still ours.
    doom_free(lnames);
}

void drawIntermission()
{
    const auto& session = gameSession();

    switch (state)
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
    const auto& version = gameVersion();

    wbs = wbstartstruct;

#ifdef RANGECHECKING
    if (version.gamemode != commercial)
    {
        if (version.gamemode == retail)
            RNGCHECK(wbs->epsd, 0, 3);
        else
            RNGCHECK(wbs->epsd, 0, 2);
    }
    else
    {
        RNGCHECK(wbs->last, 0, 8);
        RNGCHECK(wbs->next, 0, 8);
    }
    RNGCHECK(wbs->pnum, 0, MAXPLAYERS);
    RNGCHECK(wbs->pnum, 0, MAXPLAYERS);
#endif

    acceleratestage = 0;
    cnt = bcnt = 0;
    firstrefresh = 1;
    me = wbs->pnum;
    plrs = wbs->plyr;

    if (!wbs->maxkills)
        wbs->maxkills = 1;

    if (!wbs->maxitems)
        wbs->maxitems = 1;

    if (!wbs->maxsecret)
        wbs->maxsecret = 1;

    if (version.gamemode != retail)
        if (wbs->epsd > 2)
            wbs->epsd -= 3;
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
