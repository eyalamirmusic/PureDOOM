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

#include <span>

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

// INITSCALEMTOF, the scale on entry, moved to UI/AutomapView.h - AutomapView's
// scale_mtof member is initialised from it, and a header cannot read a constant
// that is file-local here.
// how much the automap moves window per tic in frame-buffer coordinates
// moves 140 pixels in 1 second
constexpr int F_PANINC = 4;
// how much zoom-in per tic
// goes to 2x in 1 second
constexpr Fixed M_ZOOMIN {(std::int32_t) (1.02 * FRACUNIT.raw)};
// how much zoom-out per tic
// pulls out to 0.5x in 1 second
constexpr Fixed M_ZOOMOUT {(std::int32_t) (FRACUNIT.raw / 1.02)};

// translates between frame-buffer and map distances
static inline Fixed frameToMap(const AutomapView& view, int x)
{
    return FixedMul(Fixed::fromInt(x), view.scale_ftom);
}

static inline int mapToFrame(Fixed x)
{
    return FixedMul(x, automapView().scale_mtof).toInt();
}

// translates between frame-buffer and map coordinates
static inline int mapXToFrame(Fixed x)
{
    return automapView().f_x + mapToFrame(x - automapView().m_x);
}

static inline int mapYToFrame(Fixed y)
{
    return automapView().f_y
           + (automapView().f_h - mapToFrame(y - automapView().m_y));
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
    Fixed slp, islp;
};

//
// The vector graphics for the automap.
// A line drawing of the player pointing right,
// starting from the middle.
//

// Scale a double by the raw unit and truncate - what the old
// static_cast<Fixed>(-.867 * R) did when Fixed was an int.
static constexpr Fixed amFixed(double value)
{
    return Fixed {(std::int32_t) value};
}
// The raw fixed-point unit, as an int32 and NOT a Fixed. Scale a `double`
// literal by this and truncate with amFixed; scaling FRACUNIT itself converts
// the literal to `int` first, so anything below 1.0 becomes zero.
constexpr std::int32_t R_UNIT = FRACUNIT.raw;

Array<MapLine, 3> triangle_guy = {
    {{amFixed(-.867 * R_UNIT), amFixed(-.5 * R_UNIT)},
     {amFixed(.867 * R_UNIT), amFixed(-.5 * R_UNIT)}},
    {{amFixed(.867 * R_UNIT), amFixed(-.5 * R_UNIT)}, {Fixed {}, Fixed {R_UNIT}}},
    {{Fixed {}, Fixed {R_UNIT}}, {amFixed(-.867 * R_UNIT), amFixed(-.5 * R_UNIT)}}};

// The automap's internal view state is a AutomapView owned by the Engine (AutomapView.h). It
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
    Fixed dy = ml.a.y - ml.b.y;
    Fixed dx = ml.b.x - ml.a.x;
    if (!dy)
        is.islp = dx.isNegative() ? Fixed {-DOOM_MAXINT} : Fixed {DOOM_MAXINT};
    else
        is.islp = FixedDiv(dx, dy);
    if (!dx)
        is.slp = dy.isNegative() ? Fixed {-DOOM_MAXINT} : Fixed {DOOM_MAXINT};
    else
        is.slp = FixedDiv(dy, dx);
}

//
//
//
void activateNewScale()
{
    auto& map = automapView();

    automapView().m_x += automapView().m_w / 2;
    automapView().m_y += automapView().m_h / 2;
    automapView().m_w = frameToMap(map, automapView().f_w);
    automapView().m_h = frameToMap(map, automapView().f_h);
    automapView().m_x -= automapView().m_w / 2;
    automapView().m_y -= automapView().m_h / 2;
    map.m_x2 = automapView().m_x + automapView().m_w;
    map.m_y2 = automapView().m_y + automapView().m_h;
}

//
//
//
void saveScaleAndLoc()
{
    auto& map = automapView();

    map.old_m_x = automapView().m_x;
    map.old_m_y = automapView().m_y;
    map.old_m_w = automapView().m_w;
    map.old_m_h = automapView().m_h;
}

//
//
//
void restoreScaleAndLoc()
{
    auto& map = automapView();

    automapView().m_w = map.old_m_w;
    automapView().m_h = map.old_m_h;
    if (!automapView().followplayer)
    {
        automapView().m_x = map.old_m_x;
        automapView().m_y = map.old_m_y;
    }
    else
    {
        automapView().m_x = automapView().am_plr->mo->x - automapView().m_w / 2;
        automapView().m_y = automapView().am_plr->mo->y - automapView().m_h / 2;
    }
    map.m_x2 = automapView().m_x + automapView().m_w;
    map.m_y2 = automapView().m_y + automapView().m_h;

    // Change the scaling multipliers
    automapView().scale_mtof =
        FixedDiv(Fixed::fromInt(automapView().f_w), automapView().m_w);
    map.scale_ftom = FixedDiv(FRACUNIT, automapView().scale_mtof);
}

//
// adds a marker at the current location
//
void addMark()
{
    auto& map = automapView();

    map.markpoints[map.markpointnum].x = automapView().m_x + automapView().m_w / 2;
    map.markpoints[map.markpointnum].y = automapView().m_y + automapView().m_h / 2;
    map.markpointnum = (map.markpointnum + 1) % AutomapView::numMarkPoints;
}

//
// Determines bounding box of all vertices,
// sets global variables controlling zoom range.
//
void findMinMaxBoundaries()
{
    auto& map = automapView();

    map.min_x = map.min_y = Fixed {DOOM_MAXINT};
    map.max_x = map.max_y = Fixed {-DOOM_MAXINT};

    for (int i = 0; i < level().vertexes.size(); i++)
    {
        if (level().vertexes[i].x < map.min_x)
            map.min_x = level().vertexes[i].x;
        else if (level().vertexes[i].x > map.max_x)
            map.max_x = level().vertexes[i].x;

        if (level().vertexes[i].y < map.min_y)
            map.min_y = level().vertexes[i].y;
        else if (level().vertexes[i].y > map.max_y)
            map.max_y = level().vertexes[i].y;
    }

    map.max_w = map.max_x - map.min_x;
    map.max_h = map.max_y - map.min_y;

    Fixed a = FixedDiv(Fixed::fromInt(automapView().f_w), map.max_w);
    Fixed b = FixedDiv(Fixed::fromInt(automapView().f_h), map.max_h);

    map.min_scale_mtof = a < b ? a : b;
    map.max_scale_mtof =
        FixedDiv(Fixed::fromInt(automapView().f_h), 2 * PLAYERRADIUS);
}

//
//
//
void changeWindowLoc()
{
    auto& map = automapView();

    if (map.m_paninc.x || map.m_paninc.y)
    {
        automapView().followplayer = 0;
        map.f_oldloc.x = Fixed {DOOM_MAXINT};
    }

    automapView().m_x += map.m_paninc.x;
    automapView().m_y += map.m_paninc.y;

    if (automapView().m_x + automapView().m_w / 2 > map.max_x)
        automapView().m_x = map.max_x - automapView().m_w / 2;
    else if (automapView().m_x + automapView().m_w / 2 < map.min_x)
        automapView().m_x = map.min_x - automapView().m_w / 2;

    if (automapView().m_y + automapView().m_h / 2 > map.max_y)
        automapView().m_y = map.max_y - automapView().m_h / 2;
    else if (automapView().m_y + automapView().m_h / 2 < map.min_y)
        automapView().m_y = map.min_y - automapView().m_h / 2;

    map.m_x2 = automapView().m_x + automapView().m_w;
    map.m_y2 = automapView().m_y + automapView().m_h;
}

//
//
//
void initAutomapVariables()
{
    auto& map = automapView();

    int pnum;
    static Event st_notify = {EventType::KeyUp, AM_MSGENTERED, 0, 0};

    overlayState().automapactive = true;
    map.fb = videoState().screens[0];

    map.f_oldloc.x = Fixed {DOOM_MAXINT};
    map.amclock = 0;
    automapView().lightlev = 0;

    map.m_paninc.x = map.m_paninc.y = Fixed {};
    map.ftom_zoommul = FRACUNIT;
    map.mtof_zoommul = FRACUNIT;

    automapView().m_w = frameToMap(map, automapView().f_w);
    automapView().m_h = frameToMap(map, automapView().f_h);

    auto& players_ = playerState();

    // find player to center on initially
    if (!players_.playeringame[pnum = players_.consoleplayer])
        for (pnum = 0; pnum < MAXPLAYERS; pnum++)
            if (players_.playeringame[pnum])
                break;

    automapView().am_plr = &players_.players[pnum];
    automapView().m_x = automapView().am_plr->mo->x - automapView().m_w / 2;
    automapView().m_y = automapView().am_plr->mo->y - automapView().m_h / 2;
    changeWindowLoc();

    // for saving & restoring
    map.old_m_x = automapView().m_x;
    map.old_m_y = automapView().m_y;
    map.old_m_w = automapView().m_w;
    map.old_m_h = automapView().m_h;

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
    // Nothing to unload any more: WadFile owns the lumps and they are
    // permanent (Wad/WadFile.h). This used to hand each patch back to the zone
    // as PU_CACHE, meaning "purge me if you need the space".
}

void clearMarks()
{
    auto& map = automapView();

    for (auto& mark: map.markpoints)
        mark.x = Fixed {-1}; // means empty
    map.markpointnum = 0;
}

//
// should be called at the start of every level
// right now, i figure it out myself
//
void levelInit()
{
    auto& map = automapView();

    automapView().f_x = automapView().f_y = 0;
    automapView().f_w = map.finit_width;
    automapView().f_h = map.finit_height;

    clearMarks();

    findMinMaxBoundaries();
    automapView().scale_mtof = FixedDiv(map.min_scale_mtof, amFixed(0.7 * R_UNIT));
    if (automapView().scale_mtof > map.max_scale_mtof)
        automapView().scale_mtof = map.min_scale_mtof;
    map.scale_ftom = FixedDiv(FRACUNIT, automapView().scale_mtof);
}

//
//
//
void stopAutomap()
{
    // Vanilla's field-shifted AM_Stop notice { 0, ev_keyup, AM_MSGEXITED }: the
    // type is 0 (keydown) and ev_keyup lands in the int data1 field, so
    // statusBarResponder's keyup test never fires - a preserved 1993 no-op. Kept
    // byte-exact: data1 holds ev_keyup's integer value.
    static Event st_notify = {static_cast<EventType>(0),
                              static_cast<int>(EventType::KeyUp),
                              AM_MSGEXITED,
                              0};

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

    automapView().scale_mtof = map.min_scale_mtof;
    map.scale_ftom = FixedDiv(FRACUNIT, automapView().scale_mtof);
    activateNewScale();
}

//
// set the window scale to the minimum size
//
void maxOutWindowScale()
{
    auto& map = automapView();

    automapView().scale_mtof = map.max_scale_mtof;
    map.scale_ftom = FixedDiv(FRACUNIT, automapView().scale_mtof);
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
        if (ev.type == EventType::KeyDown && ev.data1 == AM_STARTKEY)
        {
            startAutomap();
            refreshFlags().viewactive = false;
            rc = true;
        }
    }

    else if (ev.type == EventType::KeyDown)
    {
        rc = true;
        switch (ev.data1)
        {
            case AM_PANRIGHTKEY: // pan right
                if (!automapView().followplayer)
                    map.m_paninc.x = frameToMap(map, F_PANINC);
                else
                    rc = false;
                break;
            case AM_PANLEFTKEY: // pan left
                if (!automapView().followplayer)
                    map.m_paninc.x = -frameToMap(map, F_PANINC);
                else
                    rc = false;
                break;
            case AM_PANUPKEY: // pan up
                if (!automapView().followplayer)
                    map.m_paninc.y = frameToMap(map, F_PANINC);
                else
                    rc = false;
                break;
            case AM_PANDOWNKEY: // pan down
                if (!automapView().followplayer)
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
                automapView().followplayer = !automapView().followplayer;
                map.f_oldloc.x = Fixed {DOOM_MAXINT};
                automapView().am_plr->message =
                    automapView().followplayer ? AMSTR_FOLLOWON : AMSTR_FOLLOWOFF;
                break;
            case AM_GRIDKEY:
                automapView().grid = !automapView().grid;
                automapView().am_plr->message =
                    automapView().grid ? AMSTR_GRIDON : AMSTR_GRIDOFF;
                break;
            case AM_MARKKEY:
                //doom_sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, markpointnum);
                buffer = concat(AMSTR_MARKEDSPOT, " ", map.markpointnum);
                automapView().am_plr->message = buffer;
                addMark();
                break;
            case AM_CLEARMARKKEY:
                clearMarks();
                automapView().am_plr->message = AMSTR_MARKSCLEARED;
                break;
            default:
                rc = false;
        }
        if (!gameSession().deathmatch && checkCheat(cheat_amap, ev.data1))
        {
            rc = false;
            automapView().cheating = (automapView().cheating + 1) % 3;
        }
    }

    else if (ev.type == EventType::KeyUp)
    {
        rc = false;
        switch (ev.data1)
        {
            case AM_PANRIGHTKEY:
                if (!automapView().followplayer)
                    map.m_paninc.x = Fixed {};
                break;
            case AM_PANLEFTKEY:
                if (!automapView().followplayer)
                    map.m_paninc.x = Fixed {};
                break;
            case AM_PANUPKEY:
                if (!automapView().followplayer)
                    map.m_paninc.y = Fixed {};
                break;
            case AM_PANDOWNKEY:
                if (!automapView().followplayer)
                    map.m_paninc.y = Fixed {};
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
    automapView().scale_mtof = FixedMul(automapView().scale_mtof, map.mtof_zoommul);
    map.scale_ftom = FixedDiv(FRACUNIT, automapView().scale_mtof);

    if (automapView().scale_mtof < map.min_scale_mtof)
        minOutWindowScale();
    else if (automapView().scale_mtof > map.max_scale_mtof)
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

    if (map.f_oldloc.x != automapView().am_plr->mo->x
        || map.f_oldloc.y != automapView().am_plr->mo->y)
    {
        automapView().m_x = frameToMap(map, mapToFrame(automapView().am_plr->mo->x))
                            - automapView().m_w / 2;
        automapView().m_y = frameToMap(map, mapToFrame(automapView().am_plr->mo->y))
                            - automapView().m_h / 2;
        map.m_x2 = automapView().m_x + automapView().m_w;
        map.m_y2 = automapView().m_y + automapView().m_h;
        map.f_oldloc.x = automapView().am_plr->mo->x;
        map.f_oldloc.y = automapView().am_plr->mo->y;
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
        automapView().lightlev = litelevels[map.litelevelscnt++];
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

    if (automapView().followplayer)
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
    doom_memset(automapView().fb, color, automapView().f_w * automapView().f_h);
}

namespace
{
// Cohen-Sutherland outcode bits, accumulated into an int per endpoint.
enum class Outcode
{
    Left = 1,
    Right = 2,
    Bottom = 4,
    Top = 8
};

int computeOutcode(int mx, int my)
{
    int oc = 0;
    if (my < 0)
        oc = withFlags(oc, Outcode::Top);
    else if (my >= automapView().f_h)
        oc = withFlags(oc, Outcode::Bottom);
    if (mx < 0)
        oc = withFlags(oc, Outcode::Left);
    else if (mx >= automapView().f_w)
        oc = withFlags(oc, Outcode::Right);
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
        outcode1 = flagBits(Outcode::Top);
    else if (ml.a.y < automapView().m_y)
        outcode1 = flagBits(Outcode::Bottom);

    if (ml.b.y > map.m_y2)
        outcode2 = flagBits(Outcode::Top);
    else if (ml.b.y < automapView().m_y)
        outcode2 = flagBits(Outcode::Bottom);

    if (outcode1 & outcode2)
        return false; // trivially outside

    if (ml.a.x < automapView().m_x)
        outcode1 = withFlags(outcode1, Outcode::Left);
    else if (ml.a.x > map.m_x2)
        outcode1 = withFlags(outcode1, Outcode::Right);

    if (ml.b.x < automapView().m_x)
        outcode2 = withFlags(outcode2, Outcode::Left);
    else if (ml.b.x > map.m_x2)
        outcode2 = withFlags(outcode2, Outcode::Right);

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
        if (hasFlag(outside, Outcode::Top))
        {
            dy = fl.a.y - fl.b.y;
            dx = fl.b.x - fl.a.x;
            tmp.x = fl.a.x + (dx * (fl.a.y)) / dy;
            tmp.y = 0;
        }
        else if (hasFlag(outside, Outcode::Bottom))
        {
            dy = fl.a.y - fl.b.y;
            dx = fl.b.x - fl.a.x;
            tmp.x = fl.a.x + (dx * (fl.a.y - automapView().f_h)) / dy;
            tmp.y = automapView().f_h - 1;
        }
        else if (hasFlag(outside, Outcode::Right))
        {
            dy = fl.b.y - fl.a.y;
            dx = fl.b.x - fl.a.x;
            tmp.y = fl.a.y + (dy * (automapView().f_w - 1 - fl.a.x)) / dx;
            tmp.x = automapView().f_w - 1;
        }
        else if (hasFlag(outside, Outcode::Left))
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
    fb[yy * automapView().f_w + xx] = cc;
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
    Fixed start = automapView().m_x;
    if (Fixed {(start - level().blockmap.origin.x).raw % blockSpacing.raw})
        start +=
            blockSpacing
            - (Fixed {(start - level().blockmap.origin.x).raw % blockSpacing.raw});
    Fixed end = automapView().m_x + automapView().m_w;

    // draw vertical gridlines
    ml.a.y = automapView().m_y;
    ml.b.y = automapView().m_y + automapView().m_h;
    for (Fixed x = start; x < end; x += blockSpacing)
    {
        ml.a.x = x;
        ml.b.x = x;
        drawMline(ml, color);
    }

    // Figure out start of horizontal gridlines
    start = automapView().m_y;
    if (Fixed {(start - level().blockmap.origin.y).raw % blockSpacing.raw})
        start +=
            blockSpacing
            - (Fixed {(start - level().blockmap.origin.y).raw % blockSpacing.raw});
    end = automapView().m_y + automapView().m_h;

    // draw horizontal gridlines
    ml.a.x = automapView().m_x;
    ml.b.x = automapView().m_x + automapView().m_w;
    for (Fixed y = start; y < end; y += blockSpacing)
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

    for (int i = 0; i < level().lines.size(); i++)
    {
        l.a.x = level().lines[i].v1->x;
        l.a.y = level().lines[i].v1->y;
        l.b.x = level().lines[i].v2->x;
        l.b.y = level().lines[i].v2->y;
        if (automapView().cheating || (level().lines[i].flags & ML_MAPPED))
        {
            if ((level().lines[i].flags & LINE_NEVERSEE) && !automapView().cheating)
                continue;
            if (!level().lines[i].backsector)
            {
                drawMline(l, WALLCOLORS + automapView().lightlev);
            }
            else
            {
                if (level().lines[i].special == 39)
                { // teleporters
                    drawMline(l, WALLCOLORS + WALLRANGE / 2);
                }
                else if (level().lines[i].flags & ML_SECRET) // secret door
                {
                    if (automapView().cheating)
                        drawMline(l, SECRETWALLCOLORS + automapView().lightlev);
                    else
                        drawMline(l, WALLCOLORS + automapView().lightlev);
                }
                else if (level().lines[i].backsector->floorheight
                         != level().lines[i].frontsector->floorheight)
                {
                    drawMline(l,
                              FDWALLCOLORS
                                  + automapView().lightlev); // floor level change
                }
                else if (level().lines[i].backsector->ceilingheight
                         != level().lines[i].frontsector->ceilingheight)
                {
                    drawMline(l,
                              CDWALLCOLORS
                                  + automapView().lightlev); // ceiling level change
                }
                else if (automapView().cheating)
                {
                    drawMline(l, TSWALLCOLORS + automapView().lightlev);
                }
            }
        }
        else if (automapView().am_plr->powers[toIndex(PowerType::AllMap)])
        {
            if (!(level().lines[i].flags & LINE_NEVERSEE))
                drawMline(l, GRAYS + 3);
        }
    }
}

//
// Rotation in 2D.
// Used to rotate player arrow line character.
//
void rotateAutomapPoint(Fixed& x, Fixed& y, Angle a)
{
    Fixed tmpx = FixedMul(x, finecosine()[a.fineIndex()])
                 - FixedMul(y, finesine()[a.fineIndex()]);

    y = FixedMul(x, finesine()[a.fineIndex()])
        + FixedMul(y, finecosine()[a.fineIndex()]);

    x = tmpx;
}

// The shape and its length arrive together as a span, so the count cannot drift
// from the table it counts - vanilla passed a pointer and a separate NUM*LINES
// constant at every call site, which is exactly the guard-and-bound pairing this
// repository has a rule against.
void drawLineCharacter(std::span<const MapLine> lineguy,
                       Fixed scale,
                       Angle angle,
                       int color,
                       Fixed x,
                       Fixed y)
{
    MapLine l;

    for (auto i = 0u; i < lineguy.size(); i++)
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
        if (automapView().cheating)
            drawLineCharacter(mapShapes().cheatPlayerArrow,
                              Fixed {},
                              automapView().am_plr->mo->angle,
                              WHITE,
                              automapView().am_plr->mo->x,
                              automapView().am_plr->mo->y);
        else
            drawLineCharacter(mapShapes().playerArrow,
                              Fixed {},
                              automapView().am_plr->mo->angle,
                              WHITE,
                              automapView().am_plr->mo->x,
                              automapView().am_plr->mo->y);
        return;
    }

    auto& players_ = playerState();

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        their_color++;
        Player* p = &players_.players[i];

        if ((session.deathmatch && !demoState().singledemo)
            && p != automapView().am_plr)
            continue;

        if (!players_.playeringame[i])
            continue;

        if (p->powers[toIndex(PowerType::Invisibility)])
            color = 246; // *close* to black
        else
            color = their_colors[their_color];

        drawLineCharacter(mapShapes().playerArrow,
                          Fixed {},
                          p->mo->angle,
                          color,
                          p->mo->x,
                          p->mo->y);
    }
}

void drawThings(int colors)
{
    for (int i = 0; i < level().sectors.size(); i++)
    {
        Mobj* t = level().sectors[i].thinglist;
        while (t)
        {
            drawLineCharacter(mapShapes().thinTriangleGuy,
                              Fixed::fromInt(16),
                              t->angle,
                              colors + automapView().lightlev,
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
        if (map.markpoints[i].x != Fixed {-1})
        {
            //      w = littleEndian(marknums[i]->width);
            //      h = littleEndian(marknums[i]->height);
            int w = 5; // because something's wrong with the wad, i guess
            int h = 6; // because something's wrong with the wad, i guess
            int fx = mapXToFrame(map.markpoints[i].x);
            int fy = mapYToFrame(map.markpoints[i].y);
            if (fx >= automapView().f_x && fx <= automapView().f_w - w
                && fy >= automapView().f_y && fy <= automapView().f_h - h)
                drawPatch(fx, fy, FB, map.marknums[i]);
        }
    }
}

void amDrawCrosshair(int color)
{
    automapView().fb[(automapView().f_w * (automapView().f_h + 1)) / 2] =
        color; // single point for now
}

void drawAutomap()
{
    if (!overlayState().automapactive)
        return;

    clearFB(BACKGROUND);
    if (automapView().grid)
        drawGrid(GRIDCOLORS);
    drawWalls();
    drawPlayers();
    if (automapView().cheating == 2)
        drawThings(THINGCOLORS);
    amDrawCrosshair(XHAIRCOLORS);

    drawAutomapMarks();

    markRect(
        automapView().f_x, automapView().f_y, automapView().f_w, automapView().f_h);
}

} // namespace Doom

// ---------------------------------------------------------------------------
// The vector shapes the automap draws the player and things with, which the eacp
// port's GPU automap reads too.
// ---------------------------------------------------------------------------

// The player arrow's radius, a Fixed. Both arrow tables are drawn to it.
constexpr Doom::Fixed R = (8 * Doom::PLAYERRADIUS) / 7;

namespace Doom
{
// The three line drawings, immutable after static init and handed out const, so
// the GPU compositor reads the same tables the software automap draws from.
//
// thinTriangleGuy scales R_UNIT, the raw int32 unit, and NOT the Fixed `R` the two
// arrows use: its vertices are fractions of the unit, and `-.5 * FRACUNIT` is
// `double * Fixed`, which converts -.5 to `int` 0 before multiplying and collapses
// the shape to a point. Scale R_UNIT and truncate with amFixed.
//
// No frame golden reaches thinTriangleGuy - drawThings needs `cheating == 2` (IDDT
// twice) and no test or demo cheats - so Automap/shapeTablesAreScaled asserts the
// vertex values directly. Change these numbers and that is what fails.
const MapShapes& mapShapes()
{
    static const MapShapes shapes = {
        // playerArrow
        {{MapLine {{-R + R / 8, Fixed {}}, {R, Fixed {}}}, // -----
          {{R, Fixed {}}, {R - R / 2, R / 4}}, // ----->
          {{R, Fixed {}}, {R - R / 2, -R / 4}},
          {{-R + R / 8, Fixed {}}, {-R - R / 8, R / 4}}, // >---->
          {{-R + R / 8, Fixed {}}, {-R - R / 8, -R / 4}},
          {{-R + 3 * R / 8, Fixed {}}, {-R + R / 8, R / 4}}, // >>--->
          {{-R + 3 * R / 8, Fixed {}}, {-R + R / 8, -R / 4}}}},

        // cheatPlayerArrow
        {{MapLine {{-R + R / 8, Fixed {}}, {R, Fixed {}}}, // -----
          {{R, Fixed {}}, {R - R / 2, R / 6}}, // ----->
          {{R, Fixed {}}, {R - R / 2, -R / 6}},
          {{-R + R / 8, Fixed {}}, {-R - R / 8, R / 6}}, // >----->
          {{-R + R / 8, Fixed {}}, {-R - R / 8, -R / 6}},
          {{-R + 3 * R / 8, Fixed {}}, {-R + R / 8, R / 6}}, // >>----->
          {{-R + 3 * R / 8, Fixed {}}, {-R + R / 8, -R / 6}},
          {{-R / 2, Fixed {}}, {-R / 2, -R / 6}}, // >>-d--->
          {{-R / 2, -R / 6}, {-R / 2 + R / 6, -R / 6}},
          {{-R / 2 + R / 6, -R / 6}, {-R / 2 + R / 6, R / 4}},
          {{-R / 6, Fixed {}}, {-R / 6, -R / 6}}, // >>-dd-->
          {{-R / 6, -R / 6}, {Fixed {}, -R / 6}},
          {{Fixed {}, -R / 6}, {Fixed {}, R / 4}},
          {{R / 6, R / 4}, {R / 6, -R / 7}}, // >>-ddt->
          {{R / 6, -R / 7}, {R / 6 + R / 32, -R / 7 - R / 32}},
          {{R / 6 + R / 32, -R / 7 - R / 32}, {R / 6 + R / 10, -R / 7}}}},

        // thinTriangleGuy
        {{MapLine {{amFixed(-.5 * R_UNIT), amFixed(-.7 * R_UNIT)},
                   {Fixed {R_UNIT}, Fixed {}}},
          {{Fixed {R_UNIT}, Fixed {}},
           {amFixed(-.5 * R_UNIT), amFixed(.7 * R_UNIT)}},
          {{amFixed(-.5 * R_UNIT), amFixed(.7 * R_UNIT)},
           {amFixed(-.5 * R_UNIT), amFixed(-.7 * R_UNIT)}}}}};

    return shapes;
}
} // namespace Doom

// The map window's position, extent, scale and frame rect, the player it follows,
// and the two cheats used to be twelve loose globals here. They are AutomapView
// members now (UI/AutomapView.h).

// automapactive (with menuactive) is a Doom::OverlayState owned by the Engine now; this is a
// reference onto it (REFACTOR.md, Step 5).
