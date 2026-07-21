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

#include "../Host/Platform.h"
#include "../Sim/Level.h"

#include "AutomapTypes.h"
#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h" // State (automapactive, viewactive).
#include "../Game/Strings.h" // Data.
#include "CheatTypes.h"
#include "../Sim/SimDefs.h"
#include "StatusBarTypes.h"
#include "../Game/DemoState.h"
#include "../Game/GameSession.h"
#include "../Game/OverlayState.h"
#include "../Game/PlayerState.h"
#include "../Game/RefreshFlags.h"
#include "../Host/Text.h"
#include "../Wad/WadFile.h"

#include "Automap.h"
#include "AutomapView.h"

#include "../Render/Video.h"
#include "Cheat.h"
#include "StatusBar.h"
#include "../Containers.h"

namespace Doom
{

// Colours, the line-drawing shapes and the view state now live in am_map.h,
// so a renderer can draw the same map without rasterising it.

// The colour ranges the map draws with are in AutomapTypes.h, alongside the
// colours themselves, because the eacp compositor reads them too.

// drawing stuff
constexpr int FB = 0;

constexpr int AM_PANDOWNKEY = KEY_DOWNARROW;
constexpr int AM_PANUPKEY = KEY_UPARROW;
constexpr int AM_PANRIGHTKEY = KEY_RIGHTARROW;
constexpr int AM_PANLEFTKEY = KEY_LEFTARROW;
constexpr int AM_ZOOMINKEY = '=';
constexpr int AM_ZOOMOUTKEY = '-';
constexpr int AM_STARTKEY = KEY_TAB;
constexpr int AM_ENDKEY = KEY_TAB;
constexpr int AM_GOBIGKEY = '0';
constexpr int AM_FOLLOWKEY = 'f';
constexpr int AM_GRIDKEY = 'g';
constexpr int AM_MARKKEY = 'm';
constexpr int AM_CLEARMARKKEY = 'c';

// scale on entry
constexpr fixed_t INITSCALEMTOF {(std::int32_t) (.2 * FRACUNIT.raw)};
// how much the automap moves window per tic in frame-buffer coordinates
// moves 140 pixels in 1 second
constexpr int F_PANINC = 4;
// how much zoom-in per tic
// goes to 2x in 1 second
constexpr fixed_t M_ZOOMIN {(std::int32_t) (1.02 * FRACUNIT.raw)};
// how much zoom-out per tic
// pulls out to 0.5x in 1 second
constexpr fixed_t M_ZOOMOUT {(std::int32_t) (FRACUNIT.raw / 1.02)};

// translates between frame-buffer and map distances
static inline fixed_t frameToMap(const AutomapView& view, int x)
{
    return FixedMul(Fixed::fromInt(x), view.scale_ftom);
}

static inline int mapToFrame(fixed_t x)
{
    return FixedMul(x, scale_mtof).toInt();
}

// translates between frame-buffer and map coordinates
static inline int mapXToFrame(fixed_t x)
{
    return f_x + mapToFrame(x - m_x);
}

static inline int mapYToFrame(fixed_t y)
{
    return f_y + (f_h - mapToFrame(y - m_y));
}

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

// Scale a double by the raw unit and truncate - what the old
// static_cast<fixed_t>(-.867 * R) did when fixed_t was an int.
static constexpr fixed_t amFixed(double value)
{
    return fixed_t {(std::int32_t) value};
}
// The raw fixed-point unit, as an int32 and NOT a Fixed. Scale a `double`
// literal by this and truncate with amFixed; scaling FRACUNIT itself converts
// the literal to `int` first, so anything below 1.0 becomes zero.
constexpr std::int32_t R_UNIT = FRACUNIT.raw;

Array<MapLine, 3> triangle_guy = {
    {{amFixed(-.867 * R_UNIT), amFixed(-.5 * R_UNIT)},
     {amFixed(.867 * R_UNIT), amFixed(-.5 * R_UNIT)}},
    {{amFixed(.867 * R_UNIT), amFixed(-.5 * R_UNIT)},
     {fixed_t {}, fixed_t {R_UNIT}}},
    {{fixed_t {}, fixed_t {R_UNIT}},
     {amFixed(-.867 * R_UNIT), amFixed(-.5 * R_UNIT)}}};

// The automap's internal view state is a Doom::AutomapView owned by the Engine (AutomapView.h). It
// used to be reached through file-scope `static T& x = automapView().x;` reference aliases (the
// arrays as references-to-array), moved in by the file-scope-statics sweep (REFACTOR.md, Step 5);
// the file-local-alias sweep (REFACTOR.md, Step 9 strand (a)) retired them - every function below
// reaches automapView() through a hoisted local instead, taken once per function (or inline where a
// function touches it exactly once). frameToMap, the one helper that read a member (scale_ftom), now
// takes the view explicitly. The "iddt" cheat below stays a file-local static.

static Array<unsigned char, 5> cheat_amap_seq = {0xb2, 0x26, 0x26, 0x2e, 0xff};
static CheatSequence cheat_amap = {{cheat_amap_seq}};

// Calculates the slope and slope according to the x-axis of a line
// segment in map coordinates (with the upright y-axis n' all) so
// that it can be used with the brain-dead drawing stuff.
void getIslope(const MapLine& ml, ISlope& is)
{
    fixed_t dy = ml.a.y - ml.b.y;
    fixed_t dx = ml.b.x - ml.a.x;
    if (!dy)
        is.islp = dx.isNegative() ? fixed_t {-DOOM_MAXINT} : fixed_t {DOOM_MAXINT};
    else
        is.islp = FixedDiv(dx, dy);
    if (!dx)
        is.slp = dy.isNegative() ? fixed_t {-DOOM_MAXINT} : fixed_t {DOOM_MAXINT};
    else
        is.slp = FixedDiv(dy, dx);
}

//
//
//
void activateNewScale()
{
    auto& map = automapView();

    m_x += m_w / 2;
    m_y += m_h / 2;
    m_w = frameToMap(map, f_w);
    m_h = frameToMap(map, f_h);
    m_x -= m_w / 2;
    m_y -= m_h / 2;
    map.m_x2 = m_x + m_w;
    map.m_y2 = m_y + m_h;
}

//
//
//
void saveScaleAndLoc()
{
    auto& map = automapView();

    map.old_m_x = m_x;
    map.old_m_y = m_y;
    map.old_m_w = m_w;
    map.old_m_h = m_h;
}

//
//
//
void restoreScaleAndLoc()
{
    auto& map = automapView();

    m_w = map.old_m_w;
    m_h = map.old_m_h;
    if (!followplayer)
    {
        m_x = map.old_m_x;
        m_y = map.old_m_y;
    }
    else
    {
        m_x = am_plr->mo->x - m_w / 2;
        m_y = am_plr->mo->y - m_h / 2;
    }
    map.m_x2 = m_x + m_w;
    map.m_y2 = m_y + m_h;

    // Change the scaling multipliers
    scale_mtof = FixedDiv(Fixed::fromInt(f_w), m_w);
    map.scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
}

//
// adds a marker at the current location
//
void addMark()
{
    auto& map = automapView();

    map.markpoints[map.markpointnum].x = m_x + m_w / 2;
    map.markpoints[map.markpointnum].y = m_y + m_h / 2;
    map.markpointnum = (map.markpointnum + 1) % AutomapView::numMarkPoints;
}

//
// Determines bounding box of all vertices,
// sets global variables controlling zoom range.
//
void findMinMaxBoundaries()
{
    auto& map = automapView();

    map.min_x = map.min_y = fixed_t {DOOM_MAXINT};
    map.max_x = map.max_y = fixed_t {-DOOM_MAXINT};

    for (int i = 0; i < numvertexes; i++)
    {
        if (vertexes[i].x < map.min_x)
            map.min_x = vertexes[i].x;
        else if (vertexes[i].x > map.max_x)
            map.max_x = vertexes[i].x;

        if (vertexes[i].y < map.min_y)
            map.min_y = vertexes[i].y;
        else if (vertexes[i].y > map.max_y)
            map.max_y = vertexes[i].y;
    }

    map.max_w = map.max_x - map.min_x;
    map.max_h = map.max_y - map.min_y;

    fixed_t a = FixedDiv(Fixed::fromInt(f_w), map.max_w);
    fixed_t b = FixedDiv(Fixed::fromInt(f_h), map.max_h);

    map.min_scale_mtof = a < b ? a : b;
    map.max_scale_mtof = FixedDiv(Fixed::fromInt(f_h), 2 * PLAYERRADIUS);
}

//
//
//
void changeWindowLoc()
{
    auto& map = automapView();

    if (map.m_paninc.x || map.m_paninc.y)
    {
        followplayer = 0;
        map.f_oldloc.x = fixed_t {DOOM_MAXINT};
    }

    m_x += map.m_paninc.x;
    m_y += map.m_paninc.y;

    if (m_x + m_w / 2 > map.max_x)
        m_x = map.max_x - m_w / 2;
    else if (m_x + m_w / 2 < map.min_x)
        m_x = map.min_x - m_w / 2;

    if (m_y + m_h / 2 > map.max_y)
        m_y = map.max_y - m_h / 2;
    else if (m_y + m_h / 2 < map.min_y)
        m_y = map.min_y - m_h / 2;

    map.m_x2 = m_x + m_w;
    map.m_y2 = m_y + m_h;
}

//
//
//
void initAutomapVariables()
{
    auto& map = automapView();

    int pnum;
    static Event st_notify = {ev_keyup, AM_MSGENTERED, 0, 0};

    overlayState().automapactive = true;
    map.fb = screens[0];

    map.f_oldloc.x = fixed_t {DOOM_MAXINT};
    map.amclock = 0;
    lightlev = 0;

    map.m_paninc.x = map.m_paninc.y = fixed_t {};
    map.ftom_zoommul = FRACUNIT;
    map.mtof_zoommul = FRACUNIT;

    m_w = frameToMap(map, f_w);
    m_h = frameToMap(map, f_h);

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
    map.old_m_x = m_x;
    map.old_m_y = m_y;
    map.old_m_w = m_w;
    map.old_m_h = m_h;

    // inform the status bar of the change
    statusBarResponder(st_notify);
}

//
//
//
void loadPics()
{
    auto& map = automapView();

    for (int i = 0; i < 10; i++)
    {
        map.marknums[i] = static_cast<Patch*>(cacheLumpName(concat("AMMNUM", i)));
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
    auto& map = automapView();

    for (auto& mark: map.markpoints)
        mark.x = fixed_t {-1}; // means empty
    map.markpointnum = 0;
}

//
// should be called at the start of every level
// right now, i figure it out myself
//
void levelInit()
{
    auto& map = automapView();

    f_x = f_y = 0;
    f_w = map.finit_width;
    f_h = map.finit_height;

    clearMarks();

    findMinMaxBoundaries();
    scale_mtof = FixedDiv(map.min_scale_mtof, amFixed(0.7 * R_UNIT));
    if (scale_mtof > map.max_scale_mtof)
        scale_mtof = map.min_scale_mtof;
    map.scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
}

//
//
//
void stopAutomap()
{
    static Event st_notify = {static_cast<EventType>(0), ev_keyup, AM_MSGEXITED, 0};

    unloadPics();
    overlayState().automapactive = false;
    statusBarResponder(st_notify);
    automapView().stopped = true;
}

//
//
//
void startAutomap()
{
    auto& map = automapView();

    if (!map.stopped)
        stopAutomap();
    map.stopped = false;
    const auto& session = gameSession();

    if (map.lastlevel != session.gamemap || map.lastepisode != session.gameepisode)
    {
        levelInit();
        map.lastlevel = session.gamemap;
        map.lastepisode = session.gameepisode;
    }
    initAutomapVariables();
    loadPics();
}

//
// set the window scale to the maximum size
//
void minOutWindowScale()
{
    auto& map = automapView();

    scale_mtof = map.min_scale_mtof;
    map.scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
    activateNewScale();
}

//
// set the window scale to the minimum size
//
void maxOutWindowScale()
{
    auto& map = automapView();

    scale_mtof = map.max_scale_mtof;
    map.scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
    activateNewScale();
}

//
// Handle events (user inputs) in automap mode
//
bool automapResponder(Event& ev)
{
    auto& map = automapView();

    static std::string buffer;

    int rc = false;

    if (!overlayState().automapactive)
    {
        if (ev.type == ev_keydown && ev.data1 == AM_STARTKEY)
        {
            startAutomap();
            refreshFlags().viewactive = false;
            rc = true;
        }
    }

    else if (ev.type == ev_keydown)
    {
        rc = true;
        switch (ev.data1)
        {
            case AM_PANRIGHTKEY: // pan right
                if (!followplayer)
                    map.m_paninc.x = frameToMap(map, F_PANINC);
                else
                    rc = false;
                break;
            case AM_PANLEFTKEY: // pan left
                if (!followplayer)
                    map.m_paninc.x = -frameToMap(map, F_PANINC);
                else
                    rc = false;
                break;
            case AM_PANUPKEY: // pan up
                if (!followplayer)
                    map.m_paninc.y = frameToMap(map, F_PANINC);
                else
                    rc = false;
                break;
            case AM_PANDOWNKEY: // pan down
                if (!followplayer)
                    map.m_paninc.y = -frameToMap(map, F_PANINC);
                else
                    rc = false;
                break;
            case AM_ZOOMOUTKEY: // zoom out
                map.mtof_zoommul = M_ZOOMOUT;
                map.ftom_zoommul = M_ZOOMIN;
                break;
            case AM_ZOOMINKEY: // zoom in
                map.mtof_zoommul = M_ZOOMIN;
                map.ftom_zoommul = M_ZOOMOUT;
                break;
            case AM_ENDKEY:
                map.bigstate = 0;
                refreshFlags().viewactive = true;
                stopAutomap();
                break;
            case AM_GOBIGKEY:
                map.bigstate = !map.bigstate;
                if (map.bigstate)
                {
                    saveScaleAndLoc();
                    minOutWindowScale();
                }
                else
                    restoreScaleAndLoc();
                break;
            case AM_FOLLOWKEY:
                followplayer = !followplayer;
                map.f_oldloc.x = fixed_t {DOOM_MAXINT};
                am_plr->message = followplayer ? AMSTR_FOLLOWON : AMSTR_FOLLOWOFF;
                break;
            case AM_GRIDKEY:
                grid = !grid;
                am_plr->message = grid ? AMSTR_GRIDON : AMSTR_GRIDOFF;
                break;
            case AM_MARKKEY:
                //doom_sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, markpointnum);
                buffer = concat(AMSTR_MARKEDSPOT, " ", map.markpointnum);
                am_plr->message = buffer;
                addMark();
                break;
            case AM_CLEARMARKKEY:
                clearMarks();
                am_plr->message = AMSTR_MARKSCLEARED;
                break;
            default:
                rc = false;
        }
        if (!gameSession().deathmatch && checkCheat(cheat_amap, ev.data1))
        {
            rc = false;
            cheating = (cheating + 1) % 3;
        }
    }

    else if (ev.type == ev_keyup)
    {
        rc = false;
        switch (ev.data1)
        {
            case AM_PANRIGHTKEY:
                if (!followplayer)
                    map.m_paninc.x = fixed_t {};
                break;
            case AM_PANLEFTKEY:
                if (!followplayer)
                    map.m_paninc.x = fixed_t {};
                break;
            case AM_PANUPKEY:
                if (!followplayer)
                    map.m_paninc.y = fixed_t {};
                break;
            case AM_PANDOWNKEY:
                if (!followplayer)
                    map.m_paninc.y = fixed_t {};
                break;
            case AM_ZOOMOUTKEY:
            case AM_ZOOMINKEY:
                map.mtof_zoommul = FRACUNIT;
                map.ftom_zoommul = FRACUNIT;
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
    auto& map = automapView();

    // Change the scaling multipliers
    scale_mtof = FixedMul(scale_mtof, map.mtof_zoommul);
    map.scale_ftom = FixedDiv(FRACUNIT, scale_mtof);

    if (scale_mtof < map.min_scale_mtof)
        minOutWindowScale();
    else if (scale_mtof > map.max_scale_mtof)
        maxOutWindowScale();
    else
        activateNewScale();
}

//
//
//
void doFollowPlayer()
{
    auto& map = automapView();

    if (map.f_oldloc.x != am_plr->mo->x || map.f_oldloc.y != am_plr->mo->y)
    {
        m_x = frameToMap(map, mapToFrame(am_plr->mo->x)) - m_w / 2;
        m_y = frameToMap(map, mapToFrame(am_plr->mo->y)) - m_h / 2;
        map.m_x2 = m_x + m_w;
        map.m_y2 = m_y + m_h;
        map.f_oldloc.x = am_plr->mo->x;
        map.f_oldloc.y = am_plr->mo->y;
    }
}

//
//
//
void updateLightLev()
{
    auto& map = automapView();

    //static int litelevels[] = { 0, 3, 5, 6, 6, 7, 7, 7 };
    static Array<int, 8> litelevels = {0, 4, 7, 10, 12, 14, 15, 15};

    // Change light level
    if (map.amclock > map.nexttic)
    {
        lightlev = litelevels[map.litelevelscnt++];
        if (map.litelevelscnt == litelevels.size())
            map.litelevelscnt = 0;
        map.nexttic = map.amclock + 6 - (map.amclock % 6);
    }
}

//
// Updates on Game Tick
//
void automapTicker()
{
    if (!overlayState().automapactive)
        return;

    auto& map = automapView();

    map.amclock++;

    if (followplayer)
        doFollowPlayer();

    // Change the zoom if necessary
    if (map.ftom_zoommul != FRACUNIT)
        changeWindowScale();

    // Change x,y location
    if (map.m_paninc.x || map.m_paninc.y)
        changeWindowLoc();

    // Update light level
    // updateLightLev();
}

//
// Clear automap frame buffer.
//
void clearFB(int color)
{
    doom_memset(automapView().fb, color, f_w * f_h);
}

namespace
{
enum
{
    LEFT = 1,
    RIGHT = 2,
    BOTTOM = 4,
    TOP = 8
};

int computeOutcode(int mx, int my)
{
    int oc = 0;
    if (my < 0)
        oc |= TOP;
    else if (my >= f_h)
        oc |= BOTTOM;
    if (mx < 0)
        oc |= LEFT;
    else if (mx >= f_w)
        oc |= RIGHT;
    return oc;
}
} // namespace

//
// Automap clipping of lines.
//
// Based on Cohen-Sutherland clipping algorithm but with a slightly
// faster reject and precalculated slopes.  If the speed is needed,
// use a hash algorithm to handle  the common cases.
//
bool clipMline(const MapLine& ml, FLine& fl)
{
    auto& map = automapView();

    int outcode1 = 0;
    int outcode2 = 0;
    int outside;

    // Initialized because the four-way clip below is an if/else-if chain with no
    // final else: it covers every bit `outside` can carry, but only because
    // computeOutcode sets no others and the loop has already established it is
    // nonzero. That is a real invariant and an invisible one, so MSVC reads the
    // chain as leaving tmp unwritten.
    FPoint tmp {};
    int dx;
    int dy;

    // do trivial rejects and outcodes
    if (ml.a.y > map.m_y2)
        outcode1 = TOP;
    else if (ml.a.y < m_y)
        outcode1 = BOTTOM;

    if (ml.b.y > map.m_y2)
        outcode2 = TOP;
    else if (ml.b.y < m_y)
        outcode2 = BOTTOM;

    if (outcode1 & outcode2)
        return false; // trivially outside

    if (ml.a.x < m_x)
        outcode1 |= LEFT;
    else if (ml.a.x > map.m_x2)
        outcode1 |= RIGHT;

    if (ml.b.x < m_x)
        outcode2 |= LEFT;
    else if (ml.b.x > map.m_x2)
        outcode2 |= RIGHT;

    if (outcode1 & outcode2)
        return false; // trivially outside

    // transform to frame-buffer coordinates.
    fl.a.x = mapXToFrame(ml.a.x);
    fl.a.y = mapYToFrame(ml.a.y);
    fl.b.x = mapXToFrame(ml.b.x);
    fl.b.y = mapYToFrame(ml.b.y);

    outcode1 = computeOutcode(fl.a.x, fl.a.y);
    outcode2 = computeOutcode(fl.b.x, fl.b.y);

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
            dy = fl.a.y - fl.b.y;
            dx = fl.b.x - fl.a.x;
            tmp.x = fl.a.x + (dx * (fl.a.y)) / dy;
            tmp.y = 0;
        }
        else if (outside & BOTTOM)
        {
            dy = fl.a.y - fl.b.y;
            dx = fl.b.x - fl.a.x;
            tmp.x = fl.a.x + (dx * (fl.a.y - f_h)) / dy;
            tmp.y = f_h - 1;
        }
        else if (outside & RIGHT)
        {
            dy = fl.b.y - fl.a.y;
            dx = fl.b.x - fl.a.x;
            tmp.y = fl.a.y + (dy * (f_w - 1 - fl.a.x)) / dx;
            tmp.x = f_w - 1;
        }
        else if (outside & LEFT)
        {
            dy = fl.b.y - fl.a.y;
            dx = fl.b.x - fl.a.x;
            tmp.y = fl.a.y + (dy * (-fl.a.x)) / dx;
            tmp.x = 0;
        }

        if (outside == outcode1)
        {
            fl.a = tmp;
            outcode1 = computeOutcode(fl.a.x, fl.a.y);
        }
        else
        {
            fl.b = tmp;
            outcode2 = computeOutcode(fl.b.x, fl.b.y);
        }

        if (outcode1 & outcode2)
            return false; // trivially outside
    }

    return true;
}

static inline void putDot(byte* fb, int xx, int yy, int cc)
{
    fb[yy * f_w + xx] = cc;
}

//
// Classic Bresenham w/ whatever optimizations needed for speed
//
void drawFline(const FLine& fl, int color)
{
    auto& map = automapView();

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
    if (fl.a.x < 0 || fl.a.x >= f_w
        || fl.a.y < 0 || fl.a.y >= f_h
        || fl.b.x < 0 || fl.b.x >= f_w
        || fl.b.y < 0 || fl.b.y >= f_h)
    {
        print("fuck ", fuck++, "\r");
        return;
    }
#endif

    dx = fl.b.x - fl.a.x;
    ax = 2 * (dx < 0 ? -dx : dx);
    sx = dx < 0 ? -1 : 1;

    dy = fl.b.y - fl.a.y;
    ay = 2 * (dy < 0 ? -dy : dy);
    sy = dy < 0 ? -1 : 1;

    x = fl.a.x;
    y = fl.a.y;

    if (ax > ay)
    {
        d = ay - ax / 2;
        while (1)
        {
            putDot(map.fb, x, y, color);
            if (x == fl.b.x)
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
            putDot(map.fb, x, y, color);
            if (y == fl.b.y)
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
void drawMline(const MapLine& ml, int color)
{
    static FLine fl;

    if (clipMline(ml, fl))
        drawFline(fl, color); // draws it on frame buffer using fb coords
}

//
// Draws flat (floor/ceiling tile) aligned grid lines.
//
void drawGrid(int color)
{
    MapLine ml;

    // One blockmap cell, in fixed-point map units.
    const auto blockSpacing = Fixed::fromInt(MAPBLOCKUNITS);

    // Figure out start of vertical gridlines
    fixed_t start = m_x;
    if (fixed_t {(start - bmaporgx).raw % blockSpacing.raw})
        start +=
            blockSpacing - (fixed_t {(start - bmaporgx).raw % blockSpacing.raw});
    fixed_t end = m_x + m_w;

    // draw vertical gridlines
    ml.a.y = m_y;
    ml.b.y = m_y + m_h;
    for (fixed_t x = start; x < end; x += blockSpacing)
    {
        ml.a.x = x;
        ml.b.x = x;
        drawMline(ml, color);
    }

    // Figure out start of horizontal gridlines
    start = m_y;
    if (fixed_t {(start - bmaporgy).raw % blockSpacing.raw})
        start +=
            blockSpacing - (fixed_t {(start - bmaporgy).raw % blockSpacing.raw});
    end = m_y + m_h;

    // draw horizontal gridlines
    ml.a.x = m_x;
    ml.b.x = m_x + m_w;
    for (fixed_t y = start; y < end; y += blockSpacing)
    {
        ml.a.y = y;
        ml.b.y = y;
        drawMline(ml, color);
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
                drawMline(l, WALLCOLORS + lightlev);
            }
            else
            {
                if (lines[i].special == 39)
                { // teleporters
                    drawMline(l, WALLCOLORS + WALLRANGE / 2);
                }
                else if (lines[i].flags & ML_SECRET) // secret door
                {
                    if (cheating)
                        drawMline(l, SECRETWALLCOLORS + lightlev);
                    else
                        drawMline(l, WALLCOLORS + lightlev);
                }
                else if (lines[i].backsector->floorheight
                         != lines[i].frontsector->floorheight)
                {
                    drawMline(l, FDWALLCOLORS + lightlev); // floor level change
                }
                else if (lines[i].backsector->ceilingheight
                         != lines[i].frontsector->ceilingheight)
                {
                    drawMline(l, CDWALLCOLORS + lightlev); // ceiling level change
                }
                else if (cheating)
                {
                    drawMline(l, TSWALLCOLORS + lightlev);
                }
            }
        }
        else if (am_plr->powers[pw_allmap])
        {
            if (!(lines[i].flags & LINE_NEVERSEE))
                drawMline(l, GRAYS + 3);
        }
    }
}

//
// Rotation in 2D.
// Used to rotate player arrow line character.
//
void rotateAutomapPoint(fixed_t& x, fixed_t& y, angle_t a)
{
    fixed_t tmpx = FixedMul(x, finecosine[a.fineIndex()])
                   - FixedMul(y, finesine[a.fineIndex()]);

    y = FixedMul(x, finesine[a.fineIndex()])
        + FixedMul(y, finecosine[a.fineIndex()]);

    x = tmpx;
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
            rotateAutomapPoint(l.a.x, l.a.y, angle);

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
            rotateAutomapPoint(l.b.x, l.b.y, angle);

        l.b.x += x;
        l.b.y += y;

        drawMline(l, color);
    }
}

void drawPlayers()
{
    static Array<int, 4> their_colors = {GREENS, GRAYS, BROWNS, REDS};
    int their_color = -1;
    int color;

    const auto& session = gameSession();

    if (!session.netgame)
    {
        if (cheating)
            drawLineCharacter(cheat_player_arrow,
                              NUMCHEATPLYRLINES,
                              fixed_t {},
                              am_plr->mo->angle,
                              WHITE,
                              am_plr->mo->x,
                              am_plr->mo->y);
        else
            drawLineCharacter(player_arrow,
                              NUMPLYRLINES,
                              fixed_t {},
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
        Player* p = &players_.players[i];

        if ((session.deathmatch && !demoState().singledemo) && p != am_plr)
            continue;

        if (!players_.playeringame[i])
            continue;

        if (p->powers[pw_invisibility])
            color = 246; // *close* to black
        else
            color = their_colors[their_color];

        drawLineCharacter(player_arrow,
                          NUMPLYRLINES,
                          fixed_t {},
                          p->mo->angle,
                          color,
                          p->mo->x,
                          p->mo->y);
    }
}

void drawThings(int colors)
{
    for (int i = 0; i < numsectors; i++)
    {
        Mobj* t = sectors[i].thinglist;
        while (t)
        {
            drawLineCharacter(thintriangle_guy,
                              NUMTHINTRIANGLEGUYLINES,
                              Fixed::fromInt(16),
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
    auto& map = automapView();

    for (int i = 0; i < AutomapView::numMarkPoints; i++)
    {
        if (map.markpoints[i].x != fixed_t {-1})
        {
            //      w = littleEndian(marknums[i]->width);
            //      h = littleEndian(marknums[i]->height);
            int w = 5; // because something's wrong with the wad, i guess
            int h = 6; // because something's wrong with the wad, i guess
            int fx = mapXToFrame(map.markpoints[i].x);
            int fy = mapYToFrame(map.markpoints[i].y);
            if (fx >= f_x && fx <= f_w - w && fy >= f_y && fy <= f_h - h)
                drawPatch(fx, fy, FB, map.marknums[i]);
        }
    }
}

void amDrawCrosshair(int color)
{
    automapView().fb[(f_w * (f_h + 1)) / 2] = color; // single point for now
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

    markRect(f_x, f_y, f_w, f_h);
}

} // namespace Doom

// ---------------------------------------------------------------------------
// Global-scope data that was am_map.cpp: the vector shapes the automap draws the
// player and things with, and the view state the eacp port's GPU automap reads.
// Stays at :: scope because those are the names it links against.
// ---------------------------------------------------------------------------

// The player arrow's radius, a Fixed. Both arrow tables are drawn to it.
constexpr fixed_t R = (8 * Doom::PLAYERRADIUS) / 7;

Doom::MapLine player_arrow[] = {
    {{-R + R / 8, fixed_t {}}, {R, fixed_t {}}}, // -----
    {{R, fixed_t {}}, {R - R / 2, R / 4}}, // ----->
    {{R, fixed_t {}}, {R - R / 2, -R / 4}},
    {{-R + R / 8, fixed_t {}}, {-R - R / 8, R / 4}}, // >---->
    {{-R + R / 8, fixed_t {}}, {-R - R / 8, -R / 4}},
    {{-R + 3 * R / 8, fixed_t {}}, {-R + R / 8, R / 4}}, // >>--->
    {{-R + 3 * R / 8, fixed_t {}}, {-R + R / 8, -R / 4}}};

Doom::MapLine cheat_player_arrow[] = {
    {{-R + R / 8, fixed_t {}}, {R, fixed_t {}}}, // -----
    {{R, fixed_t {}}, {R - R / 2, R / 6}}, // ----->
    {{R, fixed_t {}}, {R - R / 2, -R / 6}},
    {{-R + R / 8, fixed_t {}}, {-R - R / 8, R / 6}}, // >----->
    {{-R + R / 8, fixed_t {}}, {-R - R / 8, -R / 6}},
    {{-R + 3 * R / 8, fixed_t {}}, {-R + R / 8, R / 6}}, // >>----->
    {{-R + 3 * R / 8, fixed_t {}}, {-R + R / 8, -R / 6}},
    {{-R / 2, fixed_t {}}, {-R / 2, -R / 6}}, // >>-d--->
    {{-R / 2, -R / 6}, {-R / 2 + R / 6, -R / 6}},
    {{-R / 2 + R / 6, -R / 6}, {-R / 2 + R / 6, R / 4}},
    {{-R / 6, fixed_t {}}, {-R / 6, -R / 6}}, // >>-dd-->
    {{-R / 6, -R / 6}, {fixed_t {}, -R / 6}},
    {{fixed_t {}, -R / 6}, {fixed_t {}, R / 4}},
    {{R / 6, R / 4}, {R / 6, -R / 7}}, // >>-ddt->
    {{R / 6, -R / 7}, {R / 6 + R / 32, -R / 7 - R / 32}},
    {{R / 6 + R / 32, -R / 7 - R / 32}, {R / 6 + R / 10, -R / 7}}};

// Scales Doom::R_UNIT, the raw int32 unit, and NOT the Fixed `R` the arrows
// above use: these vertices are fractions of the unit, and `-.5 * FRACUNIT` is
// `double * Fixed`, which converts -.5 to `int` 0 before multiplying and
// collapses the shape to a point. Scale R_UNIT and truncate with amFixed.
//
// No frame golden reaches this table - drawThings needs `cheating == 2` (IDDT
// twice) and no test or demo cheats - so Automap/shapeTablesAreScaled asserts
// the vertex values directly. Change these numbers and that is what fails.
Doom::MapLine thintriangle_guy[] = {
    {{Doom::amFixed(-.5 * Doom::R_UNIT), Doom::amFixed(-.7 * Doom::R_UNIT)},
     {fixed_t {Doom::R_UNIT}, fixed_t {}}},
    {{fixed_t {Doom::R_UNIT}, fixed_t {}},
     {Doom::amFixed(-.5 * Doom::R_UNIT), Doom::amFixed(.7 * Doom::R_UNIT)}},
    {{Doom::amFixed(-.5 * Doom::R_UNIT), Doom::amFixed(.7 * Doom::R_UNIT)},
     {Doom::amFixed(-.5 * Doom::R_UNIT), Doom::amFixed(-.7 * Doom::R_UNIT)}}};

// Map-window position/size and scale (map coords), read by the GPU automap.
fixed_t m_x, m_y;
fixed_t m_w;
fixed_t m_h;

fixed_t scale_mtof = Doom::INITSCALEMTOF;

// Frame-window position/size (screen coords).
int f_x;
int f_y;
int f_w;
int f_h;

Doom::Player* am_plr; // the player represented by an arrow
int followplayer = 1; // whether to follow the player around
int cheating = 0;
int grid = 0;
int lightlev; // used for funky strobing effect

// automapactive (with menuactive) is a Doom::OverlayState owned by the Engine now; this is a
// reference onto it (REFACTOR.md, Step 5).
