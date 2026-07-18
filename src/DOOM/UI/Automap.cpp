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
//
// $Log:$
//
// DESCRIPTION:  the automap code
//
//-----------------------------------------------------------------------------

// Rewritten out of vanilla am_map into namespace Doom.
//
// The automap: the map/frame windows, the pan/zoom, the BSP-less line walk, and
// the player/thing shapes. am_map.cpp shims the AM_ names and owns the state the
// GPU automap path reads (the shapes, windows, scale, am_plr, followplayer,
// cheating, grid, lightlev, automapactive); everything else is file-local here.
// No demo opens the automap, so this is a faithful transcription.

#include "../doom_config.h"

#include "../am_map.h"
#include "../doomdef.h"
#include "../doomstat.h" // State (automapactive, viewactive).
#include "../dstrings.h" // Data.
#include "../m_cheat.h"
#include "../p_local.h"
#include "../r_state.h" // State.
#include "../st_stuff.h"
#include "../v_video.h" // Needs access to LFB, Doom::markRect.
#include "../Game/DemoState.h"
#include "../Game/GameSession.h"
#include "../Game/OverlayState.h"
#include "../Game/PlayerState.h"
#include "../Game/RefreshFlags.h"
#include "../Wad/WadFile.h"

#include "Automap.h"
#include "AutomapView.h"

#include "../Render/Video.h"
#include "Cheat.h"
#include "StatusBar.h"
#include <ea_data_structures/Structures/Array.h>

namespace Doom
{

// Colours, the line-drawing shapes and the view state now live in am_map.h,
// so a renderer can draw the same map without rasterising it.

#define YOURRANGE 0
#define TSWALLRANGE GRAYSRANGE
#define FDWALLRANGE BROWNRANGE
#define CDWALLRANGE YELLOWRANGE
#define THINGRANGE GREENRANGE
#define SECRETWALLRANGE WALLRANGE
#define GRIDRANGE 0

// drawing stuff
#define FB 0

#define AM_PANDOWNKEY KEY_DOWNARROW
#define AM_PANUPKEY KEY_UPARROW
#define AM_PANRIGHTKEY KEY_RIGHTARROW
#define AM_PANLEFTKEY KEY_LEFTARROW
#define AM_ZOOMINKEY '='
#define AM_ZOOMOUTKEY '-'
#define AM_STARTKEY KEY_TAB
#define AM_ENDKEY KEY_TAB
#define AM_GOBIGKEY '0'
#define AM_FOLLOWKEY 'f'
#define AM_GRIDKEY 'g'
#define AM_MARKKEY 'm'
#define AM_CLEARMARKKEY 'c'

#define AM_NUMMARKPOINTS 10

// scale on entry
#define INITSCALEMTOF (.2 * FRACUNIT)
// how much the automap moves window per tic in frame-buffer coordinates
// moves 140 pixels in 1 second
#define F_PANINC 4
// how much zoom-in per tic
// goes to 2x in 1 second
#define M_ZOOMIN ((int) (1.02 * FRACUNIT))
// how much zoom-out per tic
// pulls out to 0.5x in 1 second
#define M_ZOOMOUT ((int) (FRACUNIT / 1.02))

// translates between frame-buffer and map distances
#define FTOM(x) FixedMul(((x) << 16), scale_ftom)
#define MTOF(x) (FixedMul((x), scale_mtof) >> 16)
// translates between frame-buffer and map coordinates
#define CXMTOF(x) (f_x + MTOF((x) - m_x))
#define CYMTOF(y) (f_y + (f_h - MTOF((y) - m_y)))

struct FPoint
{
    int x, y;
};

struct FLine
{
    FPoint a, b;
};

struct ISlope
{
    fixed_t slp, islp;
};

//
// The vector graphics for the automap.
// A line drawing of the player pointing right,
// starting from the middle.
//

#define R (FRACUNIT)
EA::Array<MapLine, 3> triangle_guy = {
    {{static_cast<fixed_t>(-.867 * R), static_cast<fixed_t>(-.5 * R)},
     {static_cast<fixed_t>(.867 * R), static_cast<fixed_t>(-.5 * R)}},
    {{static_cast<fixed_t>(.867 * R), static_cast<fixed_t>(-.5 * R)}, {0, R}},
    {{0, R}, {static_cast<fixed_t>(-.867 * R), static_cast<fixed_t>(-.5 * R)}}};
#undef R
#define NUMTRIANGLEGUYLINES (sizeof(triangle_guy) / sizeof(MapLine))

// The automap's internal view state is a Doom::AutomapView owned by the Engine now, moved by the
// file-scope-statics sweep; these names are references onto the members, the arrays as
// references-to-array (REFACTOR.md, Step 5). The "iddt" cheat below stays a file-local static.
static int& leveljuststarted =
    automapView().leveljuststarted; // kluge until AM_LevelInit

static int& finit_width = automapView().finit_width;
static int& finit_height = automapView().finit_height;

static byte*& fb = automapView().fb; // pseudo-frame buffer
static int& amclock = automapView().amclock;

static MapPoint& m_paninc =
    automapView().m_paninc; // window pan per tic (map coords)
static fixed_t& mtof_zoommul =
    automapView().mtof_zoommul; // window zoom per tic (map)
static fixed_t& ftom_zoommul =
    automapView().ftom_zoommul; // window zoom per tic (fb)

static fixed_t& m_x2 =
    automapView().m_x2; // UR corner where the window is (map coords)
static fixed_t& m_y2 = automapView().m_y2;

//
// width/height of window on map (map coords)
//

// based on level size
static fixed_t& min_x = automapView().min_x;
static fixed_t& min_y = automapView().min_y;
static fixed_t& max_x = automapView().max_x;
static fixed_t& max_y = automapView().max_y;

static fixed_t& max_w = automapView().max_w; // max_x-min_x,
static fixed_t& max_h = automapView().max_h; // max_y-min_y

// based on player size
static fixed_t& min_w = automapView().min_w;
static fixed_t& min_h = automapView().min_h;

static fixed_t& min_scale_mtof = automapView().min_scale_mtof; // stop zooming out
static fixed_t& max_scale_mtof = automapView().max_scale_mtof; // stop zooming in

// old stuff for recovery later
static fixed_t& old_m_w = automapView().old_m_w;
static fixed_t& old_m_h = automapView().old_m_h;
static fixed_t& old_m_x = automapView().old_m_x;
static fixed_t& old_m_y = automapView().old_m_y;

// old location used by the Follower routine
static MapPoint& f_oldloc = automapView().f_oldloc;

// used by MTOF/FTOM to scale between map and frame-buffer coords (=1/scale_mtof)
static fixed_t& scale_ftom = automapView().scale_ftom;

static Patch* (&marknums)[10] = automapView().marknums; // mark-number patches
static MapPoint (&markpoints)[AM_NUMMARKPOINTS] =
    automapView().markpoints; // the marks
static int& markpointnum = automapView().markpointnum; // next point to be assigned

static EA::Array<unsigned char, 5> cheat_amap_seq = {0xb2, 0x26, 0x26, 0x2e, 0xff};
static CheatSequence cheat_amap = {cheat_amap_seq.data(), 0};

static doom_boolean& stopped = automapView().stopped;

// Calculates the slope and slope according to the x-axis of a line
// segment in map coordinates (with the upright y-axis n' all) so
// that it can be used with the brain-dead drawing stuff.
void getIslope(MapLine* ml, ISlope* is)
{
    int dx, dy;

    dy = ml->a.y - ml->b.y;
    dx = ml->b.x - ml->a.x;
    if (!dy)
        is->islp = (dx < 0 ? -DOOM_MAXINT : DOOM_MAXINT);
    else
        is->islp = FixedDiv(dx, dy);
    if (!dx)
        is->slp = (dy < 0 ? -DOOM_MAXINT : DOOM_MAXINT);
    else
        is->slp = FixedDiv(dy, dx);
}

//
//
//
void activateNewScale()
{
    m_x += m_w / 2;
    m_y += m_h / 2;
    m_w = FTOM(f_w);
    m_h = FTOM(f_h);
    m_x -= m_w / 2;
    m_y -= m_h / 2;
    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;
}

//
//
//
void saveScaleAndLoc()
{
    old_m_x = m_x;
    old_m_y = m_y;
    old_m_w = m_w;
    old_m_h = m_h;
}

//
//
//
void restoreScaleAndLoc()
{
    m_w = old_m_w;
    m_h = old_m_h;
    if (!followplayer)
    {
        m_x = old_m_x;
        m_y = old_m_y;
    }
    else
    {
        m_x = am_plr->mo->x - m_w / 2;
        m_y = am_plr->mo->y - m_h / 2;
    }
    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;

    // Change the scaling multipliers
    scale_mtof = FixedDiv(f_w << FRACBITS, m_w);
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
}

//
// adds a marker at the current location
//
void addMark()
{
    markpoints[markpointnum].x = m_x + m_w / 2;
    markpoints[markpointnum].y = m_y + m_h / 2;
    markpointnum = (markpointnum + 1) % AM_NUMMARKPOINTS;
}

//
// Determines bounding box of all vertices,
// sets global variables controlling zoom range.
//
void findMinMaxBoundaries()
{
    fixed_t a;
    fixed_t b;

    min_x = min_y = DOOM_MAXINT;
    max_x = max_y = -DOOM_MAXINT;

    for (int i = 0; i < numvertexes; i++)
    {
        if (vertexes[i].x < min_x)
            min_x = vertexes[i].x;
        else if (vertexes[i].x > max_x)
            max_x = vertexes[i].x;

        if (vertexes[i].y < min_y)
            min_y = vertexes[i].y;
        else if (vertexes[i].y > max_y)
            max_y = vertexes[i].y;
    }

    max_w = max_x - min_x;
    max_h = max_y - min_y;

    min_w = 2 * PLAYERRADIUS; // const? never changed?
    min_h = 2 * PLAYERRADIUS;

    a = FixedDiv(f_w << FRACBITS, max_w);
    b = FixedDiv(f_h << FRACBITS, max_h);

    min_scale_mtof = a < b ? a : b;
    max_scale_mtof = FixedDiv(f_h << FRACBITS, 2 * PLAYERRADIUS);
}

//
//
//
void changeWindowLoc()
{
    if (m_paninc.x || m_paninc.y)
    {
        followplayer = 0;
        f_oldloc.x = DOOM_MAXINT;
    }

    m_x += m_paninc.x;
    m_y += m_paninc.y;

    if (m_x + m_w / 2 > max_x)
        m_x = max_x - m_w / 2;
    else if (m_x + m_w / 2 < min_x)
        m_x = min_x - m_w / 2;

    if (m_y + m_h / 2 > max_y)
        m_y = max_y - m_h / 2;
    else if (m_y + m_h / 2 < min_y)
        m_y = min_y - m_h / 2;

    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;
}

//
//
//
void initAutomapVariables()
{
    int pnum;
    static Event st_notify = {ev_keyup, AM_MSGENTERED, 0, 0};

    overlayState().automapactive = true;
    fb = screens[0];

    f_oldloc.x = DOOM_MAXINT;
    amclock = 0;
    lightlev = 0;

    m_paninc.x = m_paninc.y = 0;
    ftom_zoommul = FRACUNIT;
    mtof_zoommul = FRACUNIT;

    m_w = FTOM(f_w);
    m_h = FTOM(f_h);

    auto& players_ = playerState();

    // find player to center on initially
    if (!players_.playeringame[pnum = players_.consoleplayer])
        for (pnum = 0; pnum < MAXPLAYERS; pnum++)
            if (players_.playeringame[pnum])
                break;

    am_plr = &players_.players[pnum];
    m_x = am_plr->mo->x - m_w / 2;
    m_y = am_plr->mo->y - m_h / 2;
    changeWindowLoc();

    // for saving & restoring
    old_m_x = m_x;
    old_m_y = m_y;
    old_m_w = m_w;
    old_m_h = m_h;

    // inform the status bar of the change
    Doom::statusBarResponder(&st_notify);
}

//
//
//
void loadPics()
{
    EA::Array<char, 9> namebuf;

    for (int i = 0; i < 10; i++)
    {
        doom_concat(doom_strcpy(namebuf.data(), "AMMNUM"), doom_itoa(i, 10));
        marknums[i] = static_cast<Patch*>(Doom::cacheLumpName(namebuf.data()));
    }
}

void unloadPics()
{
    // Nothing to unload any more: Doom::WadFile owns the lumps and they are
    // permanent (Wad/WadFile.h). This used to hand each patch back to the zone
    // as PU_CACHE, meaning "purge me if you need the space".
}

void clearMarks()
{
    for (int i = 0; i < AM_NUMMARKPOINTS; i++)
        markpoints[i].x = -1; // means empty
    markpointnum = 0;
}

//
// should be called at the start of every level
// right now, i figure it out myself
//
void levelInit()
{
    leveljuststarted = 0;

    f_x = f_y = 0;
    f_w = finit_width;
    f_h = finit_height;

    clearMarks();

    findMinMaxBoundaries();
    scale_mtof = FixedDiv(min_scale_mtof, static_cast<int>(0.7 * FRACUNIT));
    if (scale_mtof > max_scale_mtof)
        scale_mtof = min_scale_mtof;
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
}

//
//
//
void stopAutomap()
{
    static Event st_notify = {static_cast<EventType>(0), ev_keyup, AM_MSGEXITED, 0};

    unloadPics();
    overlayState().automapactive = false;
    Doom::statusBarResponder(&st_notify);
    stopped = true;
}

//
//
//
void startAutomap()
{
    int& lastlevel = automapView().lastlevel;
    int& lastepisode = automapView().lastepisode;

    if (!stopped)
        stopAutomap();
    stopped = false;
    const auto& session = gameSession();

    if (lastlevel != session.gamemap || lastepisode != session.gameepisode)
    {
        levelInit();
        lastlevel = session.gamemap;
        lastepisode = session.gameepisode;
    }
    initAutomapVariables();
    loadPics();
}

//
// set the window scale to the maximum size
//
void minOutWindowScale()
{
    scale_mtof = min_scale_mtof;
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
    activateNewScale();
}

//
// set the window scale to the minimum size
//
void maxOutWindowScale()
{
    scale_mtof = max_scale_mtof;
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
    activateNewScale();
}

//
// Handle events (user inputs) in automap mode
//
doom_boolean automapResponder(Event* ev)
{
    int rc;
    int& bigstate = automapView().bigstate;
    static EA::Array<char, 20> buffer;

    rc = false;

    if (!overlayState().automapactive)
    {
        if (ev->type == ev_keydown && ev->data1 == AM_STARTKEY)
        {
            startAutomap();
            refreshFlags().viewactive = false;
            rc = true;
        }
    }

    else if (ev->type == ev_keydown)
    {
        rc = true;
        switch (ev->data1)
        {
            case AM_PANRIGHTKEY: // pan right
                if (!followplayer)
                    m_paninc.x = FTOM(F_PANINC);
                else
                    rc = false;
                break;
            case AM_PANLEFTKEY: // pan left
                if (!followplayer)
                    m_paninc.x = -FTOM(F_PANINC);
                else
                    rc = false;
                break;
            case AM_PANUPKEY: // pan up
                if (!followplayer)
                    m_paninc.y = FTOM(F_PANINC);
                else
                    rc = false;
                break;
            case AM_PANDOWNKEY: // pan down
                if (!followplayer)
                    m_paninc.y = -FTOM(F_PANINC);
                else
                    rc = false;
                break;
            case AM_ZOOMOUTKEY: // zoom out
                mtof_zoommul = M_ZOOMOUT;
                ftom_zoommul = M_ZOOMIN;
                break;
            case AM_ZOOMINKEY: // zoom in
                mtof_zoommul = M_ZOOMIN;
                ftom_zoommul = M_ZOOMOUT;
                break;
            case AM_ENDKEY:
                bigstate = 0;
                refreshFlags().viewactive = true;
                stopAutomap();
                break;
            case AM_GOBIGKEY:
                bigstate = !bigstate;
                if (bigstate)
                {
                    saveScaleAndLoc();
                    minOutWindowScale();
                }
                else
                    restoreScaleAndLoc();
                break;
            case AM_FOLLOWKEY:
                followplayer = !followplayer;
                f_oldloc.x = DOOM_MAXINT;
                am_plr->message = const_cast<char*>(followplayer ? AMSTR_FOLLOWON
                                                                 : AMSTR_FOLLOWOFF);
                break;
            case AM_GRIDKEY:
                grid = !grid;
                am_plr->message =
                    const_cast<char*>(grid ? AMSTR_GRIDON : AMSTR_GRIDOFF);
                break;
            case AM_MARKKEY:
                doom_strcpy(buffer.data(), AMSTR_MARKEDSPOT);
                doom_concat(buffer.data(), " ");
                doom_concat(buffer.data(), doom_itoa(markpointnum, 10));
                //doom_sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, markpointnum);
                am_plr->message = buffer.data();
                addMark();
                break;
            case AM_CLEARMARKKEY:
                clearMarks();
                am_plr->message = AMSTR_MARKSCLEARED;
                break;
            default:
                rc = false;
        }
        if (!gameSession().deathmatch && Doom::checkCheat(&cheat_amap, ev->data1))
        {
            rc = false;
            cheating = (cheating + 1) % 3;
        }
    }

    else if (ev->type == ev_keyup)
    {
        rc = false;
        switch (ev->data1)
        {
            case AM_PANRIGHTKEY:
                if (!followplayer)
                    m_paninc.x = 0;
                break;
            case AM_PANLEFTKEY:
                if (!followplayer)
                    m_paninc.x = 0;
                break;
            case AM_PANUPKEY:
                if (!followplayer)
                    m_paninc.y = 0;
                break;
            case AM_PANDOWNKEY:
                if (!followplayer)
                    m_paninc.y = 0;
                break;
            case AM_ZOOMOUTKEY:
            case AM_ZOOMINKEY:
                mtof_zoommul = FRACUNIT;
                ftom_zoommul = FRACUNIT;
                break;
        }
    }

    return rc;
}

//
// Zooming
//
void changeWindowScale()
{
    // Change the scaling multipliers
    scale_mtof = FixedMul(scale_mtof, mtof_zoommul);
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);

    if (scale_mtof < min_scale_mtof)
        minOutWindowScale();
    else if (scale_mtof > max_scale_mtof)
        maxOutWindowScale();
    else
        activateNewScale();
}

//
//
//
void doFollowPlayer()
{
    if (f_oldloc.x != am_plr->mo->x || f_oldloc.y != am_plr->mo->y)
    {
        m_x = FTOM(MTOF(am_plr->mo->x)) - m_w / 2;
        m_y = FTOM(MTOF(am_plr->mo->y)) - m_h / 2;
        m_x2 = m_x + m_w;
        m_y2 = m_y + m_h;
        f_oldloc.x = am_plr->mo->x;
        f_oldloc.y = am_plr->mo->y;
    }
}

//
//
//
void updateLightLev()
{
    int& nexttic = automapView().nexttic;
    //static int litelevels[] = { 0, 3, 5, 6, 6, 7, 7, 7 };
    static EA::Array<int, 8> litelevels = {0, 4, 7, 10, 12, 14, 15, 15};
    int& litelevelscnt = automapView().litelevelscnt;

    // Change light level
    if (amclock > nexttic)
    {
        lightlev = litelevels[litelevelscnt++];
        if (litelevelscnt == litelevels.size())
            litelevelscnt = 0;
        nexttic = amclock + 6 - (amclock % 6);
    }
}

//
// Updates on Game Tick
//
void automapTicker()
{
    if (!overlayState().automapactive)
        return;

    amclock++;

    if (followplayer)
        doFollowPlayer();

    // Change the zoom if necessary
    if (ftom_zoommul != FRACUNIT)
        changeWindowScale();

    // Change x,y location
    if (m_paninc.x || m_paninc.y)
        changeWindowLoc();

    // Update light level
    // updateLightLev();
}

//
// Clear automap frame buffer.
//
void clearFB(int color)
{
    doom_memset(fb, color, f_w * f_h);
}

//
// Automap clipping of lines.
//
// Based on Cohen-Sutherland clipping algorithm but with a slightly
// faster reject and precalculated slopes.  If the speed is needed,
// use a hash algorithm to handle  the common cases.
//
doom_boolean clipMline(MapLine* ml, FLine* fl)
{
    enum
    {
        LEFT = 1,
        RIGHT = 2,
        BOTTOM = 4,
        TOP = 8
    };

    int outcode1 = 0;
    int outcode2 = 0;
    int outside;

    FPoint tmp;
    int dx;
    int dy;

#define DOOUTCODE(oc, mx, my)                                                       \
    (oc) = 0;                                                                       \
    if ((my) < 0)                                                                   \
        (oc) |= TOP;                                                                \
    else if ((my) >= f_h)                                                           \
        (oc) |= BOTTOM;                                                             \
    if ((mx) < 0)                                                                   \
        (oc) |= LEFT;                                                               \
    else if ((mx) >= f_w)                                                           \
        (oc) |= RIGHT;

    // do trivial rejects and outcodes
    if (ml->a.y > m_y2)
        outcode1 = TOP;
    else if (ml->a.y < m_y)
        outcode1 = BOTTOM;

    if (ml->b.y > m_y2)
        outcode2 = TOP;
    else if (ml->b.y < m_y)
        outcode2 = BOTTOM;

    if (outcode1 & outcode2)
        return false; // trivially outside

    if (ml->a.x < m_x)
        outcode1 |= LEFT;
    else if (ml->a.x > m_x2)
        outcode1 |= RIGHT;

    if (ml->b.x < m_x)
        outcode2 |= LEFT;
    else if (ml->b.x > m_x2)
        outcode2 |= RIGHT;

    if (outcode1 & outcode2)
        return false; // trivially outside

    // transform to frame-buffer coordinates.
    fl->a.x = CXMTOF(ml->a.x);
    fl->a.y = CYMTOF(ml->a.y);
    fl->b.x = CXMTOF(ml->b.x);
    fl->b.y = CYMTOF(ml->b.y);

    DOOUTCODE(outcode1, fl->a.x, fl->a.y);
    DOOUTCODE(outcode2, fl->b.x, fl->b.y);

    if (outcode1 & outcode2)
        return false;

    while (outcode1 | outcode2)
    {
        // may be partially inside box
        // find an outside point
        if (outcode1)
            outside = outcode1;
        else
            outside = outcode2;

        // clip to each side
        if (outside & TOP)
        {
            dy = fl->a.y - fl->b.y;
            dx = fl->b.x - fl->a.x;
            tmp.x = fl->a.x + (dx * (fl->a.y)) / dy;
            tmp.y = 0;
        }
        else if (outside & BOTTOM)
        {
            dy = fl->a.y - fl->b.y;
            dx = fl->b.x - fl->a.x;
            tmp.x = fl->a.x + (dx * (fl->a.y - f_h)) / dy;
            tmp.y = f_h - 1;
        }
        else if (outside & RIGHT)
        {
            dy = fl->b.y - fl->a.y;
            dx = fl->b.x - fl->a.x;
            tmp.y = fl->a.y + (dy * (f_w - 1 - fl->a.x)) / dx;
            tmp.x = f_w - 1;
        }
        else if (outside & LEFT)
        {
            dy = fl->b.y - fl->a.y;
            dx = fl->b.x - fl->a.x;
            tmp.y = fl->a.y + (dy * (-fl->a.x)) / dx;
            tmp.x = 0;
        }

        if (outside == outcode1)
        {
            fl->a = tmp;
            DOOUTCODE(outcode1, fl->a.x, fl->a.y);
        }
        else
        {
            fl->b = tmp;
            DOOUTCODE(outcode2, fl->b.x, fl->b.y);
        }

        if (outcode1 & outcode2)
            return false; // trivially outside
    }

    return true;
}
#undef DOOUTCODE

//
// Classic Bresenham w/ whatever optimizations needed for speed
//
void drawFline(FLine* fl, int color)
{
    int x;
    int y;
    int dx;
    int dy;
    int sx;
    int sy;
    int ax;
    int ay;
    int d;

    // For debugging only
#if 0 // [pd] Don't waste CPU cycles testing this then
    if (fl->a.x < 0 || fl->a.x >= f_w
        || fl->a.y < 0 || fl->a.y >= f_h
        || fl->b.x < 0 || fl->b.x >= f_w
        || fl->b.y < 0 || fl->b.y >= f_h)
    {
        doom_print("fuck ");
        doom_print(doom_itoa(fuck++, 10));
        doom_print("\r");
        return;
    }
#endif

#define PUTDOT(xx, yy, cc) fb[(yy) * f_w + (xx)] = (cc)

    dx = fl->b.x - fl->a.x;
    ax = 2 * (dx < 0 ? -dx : dx);
    sx = dx < 0 ? -1 : 1;

    dy = fl->b.y - fl->a.y;
    ay = 2 * (dy < 0 ? -dy : dy);
    sy = dy < 0 ? -1 : 1;

    x = fl->a.x;
    y = fl->a.y;

    if (ax > ay)
    {
        d = ay - ax / 2;
        while (1)
        {
            PUTDOT(x, y, color);
            if (x == fl->b.x)
                return;
            if (d >= 0)
            {
                y += sy;
                d -= ax;
            }
            x += sx;
            d += ay;
        }
    }
    else
    {
        d = ax - ay / 2;
        while (1)
        {
            PUTDOT(x, y, color);
            if (y == fl->b.y)
                return;
            if (d >= 0)
            {
                x += sx;
                d -= ay;
            }
            y += sy;
            d += ax;
        }
    }
}

//
// Clip lines, draw visible part sof lines.
//
void drawMline(MapLine* ml, int color)
{
    static FLine fl;

    if (clipMline(ml, &fl))
        drawFline(&fl, color); // draws it on frame buffer using fb coords
}

//
// Draws flat (floor/ceiling tile) aligned grid lines.
//
void drawGrid(int color)
{
    fixed_t x, y;
    fixed_t start, end;
    MapLine ml;

    // Figure out start of vertical gridlines
    start = m_x;
    if ((start - bmaporgx) % (MAPBLOCKUNITS << FRACBITS))
        start += (MAPBLOCKUNITS << FRACBITS)
                 - ((start - bmaporgx) % (MAPBLOCKUNITS << FRACBITS));
    end = m_x + m_w;

    // draw vertical gridlines
    ml.a.y = m_y;
    ml.b.y = m_y + m_h;
    for (x = start; x < end; x += (MAPBLOCKUNITS << FRACBITS))
    {
        ml.a.x = x;
        ml.b.x = x;
        drawMline(&ml, color);
    }

    // Figure out start of horizontal gridlines
    start = m_y;
    if ((start - bmaporgy) % (MAPBLOCKUNITS << FRACBITS))
        start += (MAPBLOCKUNITS << FRACBITS)
                 - ((start - bmaporgy) % (MAPBLOCKUNITS << FRACBITS));
    end = m_y + m_h;

    // draw horizontal gridlines
    ml.a.x = m_x;
    ml.b.x = m_x + m_w;
    for (y = start; y < end; y += (MAPBLOCKUNITS << FRACBITS))
    {
        ml.a.y = y;
        ml.b.y = y;
        drawMline(&ml, color);
    }
}

//
// Determines visible lines, draws them.
// This is LineDef based, not LineSeg based.
//
void drawWalls()
{
    static MapLine l;

    for (int i = 0; i < numlines; i++)
    {
        l.a.x = lines[i].v1->x;
        l.a.y = lines[i].v1->y;
        l.b.x = lines[i].v2->x;
        l.b.y = lines[i].v2->y;
        if (cheating || (lines[i].flags & ML_MAPPED))
        {
            if ((lines[i].flags & LINE_NEVERSEE) && !cheating)
                continue;
            if (!lines[i].backsector)
            {
                drawMline(&l, WALLCOLORS + lightlev);
            }
            else
            {
                if (lines[i].special == 39)
                { // teleporters
                    drawMline(&l, WALLCOLORS + WALLRANGE / 2);
                }
                else if (lines[i].flags & ML_SECRET) // secret door
                {
                    if (cheating)
                        drawMline(&l, SECRETWALLCOLORS + lightlev);
                    else
                        drawMline(&l, WALLCOLORS + lightlev);
                }
                else if (lines[i].backsector->floorheight
                         != lines[i].frontsector->floorheight)
                {
                    drawMline(&l, FDWALLCOLORS + lightlev); // floor level change
                }
                else if (lines[i].backsector->ceilingheight
                         != lines[i].frontsector->ceilingheight)
                {
                    drawMline(&l, CDWALLCOLORS + lightlev); // ceiling level change
                }
                else if (cheating)
                {
                    drawMline(&l, TSWALLCOLORS + lightlev);
                }
            }
        }
        else if (am_plr->powers[pw_allmap])
        {
            if (!(lines[i].flags & LINE_NEVERSEE))
                drawMline(&l, GRAYS + 3);
        }
    }
}

//
// Rotation in 2D.
// Used to rotate player arrow line character.
//
void rotateAutomapPoint(fixed_t* x, fixed_t* y, angle_t a)
{
    fixed_t tmpx;

    tmpx = FixedMul(*x, finecosine[a >> ANGLETOFINESHIFT])
           - FixedMul(*y, finesine[a >> ANGLETOFINESHIFT]);

    *y = FixedMul(*x, finesine[a >> ANGLETOFINESHIFT])
         + FixedMul(*y, finecosine[a >> ANGLETOFINESHIFT]);

    *x = tmpx;
}

void drawLineCharacter(MapLine* lineguy,
                       int lineguylines,
                       fixed_t scale,
                       angle_t angle,
                       int color,
                       fixed_t x,
                       fixed_t y)
{
    MapLine l;

    for (int i = 0; i < lineguylines; i++)
    {
        l.a.x = lineguy[i].a.x;
        l.a.y = lineguy[i].a.y;

        if (scale)
        {
            l.a.x = FixedMul(scale, l.a.x);
            l.a.y = FixedMul(scale, l.a.y);
        }

        if (angle)
            rotateAutomapPoint(&l.a.x, &l.a.y, angle);

        l.a.x += x;
        l.a.y += y;

        l.b.x = lineguy[i].b.x;
        l.b.y = lineguy[i].b.y;

        if (scale)
        {
            l.b.x = FixedMul(scale, l.b.x);
            l.b.y = FixedMul(scale, l.b.y);
        }

        if (angle)
            rotateAutomapPoint(&l.b.x, &l.b.y, angle);

        l.b.x += x;
        l.b.y += y;

        drawMline(&l, color);
    }
}

void drawPlayers()
{
    Player* p;
    static EA::Array<int, 4> their_colors = {GREENS, GRAYS, BROWNS, REDS};
    int their_color = -1;
    int color;

    const auto& session = gameSession();

    if (!session.netgame)
    {
        if (cheating)
            drawLineCharacter(cheat_player_arrow,
                              NUMCHEATPLYRLINES,
                              0,
                              am_plr->mo->angle,
                              WHITE,
                              am_plr->mo->x,
                              am_plr->mo->y);
        else
            drawLineCharacter(player_arrow,
                              NUMPLYRLINES,
                              0,
                              am_plr->mo->angle,
                              WHITE,
                              am_plr->mo->x,
                              am_plr->mo->y);
        return;
    }

    auto& players_ = playerState();

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        their_color++;
        p = &players_.players[i];

        if ((session.deathmatch && !demoState().singledemo) && p != am_plr)
            continue;

        if (!players_.playeringame[i])
            continue;

        if (p->powers[pw_invisibility])
            color = 246; // *close* to black
        else
            color = their_colors[their_color];

        drawLineCharacter(
            player_arrow, NUMPLYRLINES, 0, p->mo->angle, color, p->mo->x, p->mo->y);
    }
}

void drawThings(int colors)
{
    Mobj* t;

    for (int i = 0; i < numsectors; i++)
    {
        t = sectors[i].thinglist;
        while (t)
        {
            drawLineCharacter(thintriangle_guy,
                              NUMTHINTRIANGLEGUYLINES,
                              16 << FRACBITS,
                              t->angle,
                              colors + lightlev,
                              t->x,
                              t->y);
            t = t->snext;
        }
    }
}

void drawAutomapMarks()
{
    int fx, fy, w, h;

    for (int i = 0; i < AM_NUMMARKPOINTS; i++)
    {
        if (markpoints[i].x != -1)
        {
            //      w = SHORT(marknums[i]->width);
            //      h = SHORT(marknums[i]->height);
            w = 5; // because something's wrong with the wad, i guess
            h = 6; // because something's wrong with the wad, i guess
            fx = CXMTOF(markpoints[i].x);
            fy = CYMTOF(markpoints[i].y);
            if (fx >= f_x && fx <= f_w - w && fy >= f_y && fy <= f_h - h)
                Doom::drawPatch(fx, fy, FB, marknums[i]);
        }
    }
}

void amDrawCrosshair(int color)
{
    fb[(f_w * (f_h + 1)) / 2] = color; // single point for now
}

void drawAutomap()
{
    if (!overlayState().automapactive)
        return;

    clearFB(BACKGROUND);
    if (grid)
        drawGrid(GRIDCOLORS);
    drawWalls();
    drawPlayers();
    if (cheating == 2)
        drawThings(THINGCOLORS);
    amDrawCrosshair(XHAIRCOLORS);

    drawAutomapMarks();

    Doom::markRect(f_x, f_y, f_w, f_h);
}

} // namespace Doom
