#pragma once

#include "AutomapTypes.h" // MapPoint (and fixed_t through it)
#include "../Game/GameDefs.h" // SCREENWIDTH, SCREENHEIGHT
#include "../doomtype.h" // byte
#include "../Sim/MapTypes.h"
#include "../Render/RenderTypes.h" // Patch

#include "../Containers.h"

namespace Doom
{
// The automap's internal view state - what UI/Automap keeps for itself, distinct from the handful
// of globals the am_map.cpp shim exports for the GPU automap to read (m_x/m_y/m_w/m_h, scale_mtof,
// f_x/f_y/f_w/f_h, the player/thing shapes, followplayer/grid/cheating). This is the rest: the
// pan/zoom increments and limits, the level's map bounds, the saved window for resize recovery, the
// follower's old location, the frame->map scale, the placed marks, and the open/closed flag. None
// is read by any other file. (Named AutomapView, not AutomapState, because st_stuff.h already has a
// global enum value AutomapState - the StatusBarMode case - which would be ambiguous with a
// Doom::AutomapState under `using namespace Doom`.)
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5). The vanilla names
// were references onto these members (the arrays as references-to-array) until the file-local-alias
// sweep (REFACTOR.md, Step 9 strand (a)) retired them; UI/Automap.cpp now reaches automapView()
// through a hoisted local per function instead. The automap has its own frame golden now
// (Tests/Goldens/automap.frames, via Tests/AutomapReplay.h - no demo opens it, so nothing else
// covers this file), which is what made retiring these aliases safe to verify by more than build +
// app-link. levelInit / initAutomapVariables reset the view before it is drawn, so the defaults
// matter little, but they reproduce vanilla's initializers.
// (The "iddt" cheat sequence stays a file-local static: its CheatSequence holds a pointer to its own
// byte array, which does not survive being a copyable struct member; it is an m_cheat concern.)
struct AutomapView
{
    static constexpr int numMarkPoints =
        10; // markpoints below, and the wrap it cycles through

    // No leveljuststarted: vanilla's own comment called it a "kluge until
    // AM_LevelInit() is called", set once by levelInit and read nowhere in either
    // era. Verified against the 1993 source in this repository's history; deleted
    // rather than carried, as no read was lost - the same basis min_w/min_h below
    // were dropped on.
    int finit_width = SCREENWIDTH; // the automap frame's width
    int finit_height = SCREENHEIGHT - 32; // ... and height (above the status bar)

    byte* fb = nullptr; // the pseudo-framebuffer the automap draws into
    int amclock = 0; // the automap's own tic clock

    // Animation / level-change detection, folded in from function-local statics:
    int lastlevel = -1; // startAutomap: last map, to re-init on change
    int lastepisode = -1; // startAutomap: last episode
    int bigstate =
        0; // Doom::automapResponder: the "big" (zoomed-out overview) toggle
    int nexttic = 0; // Doom::automapTicker: next tic the fuse animation advances
    int litelevelscnt =
        0; // Doom::automapTicker: cursor into the fuse brightness ramp

    MapPoint m_paninc = {}; // window pan per tic (map coords)
    fixed_t mtof_zoommul {}; // window zoom per tic (map -> frame)
    fixed_t ftom_zoommul {}; // window zoom per tic (frame -> map)

    fixed_t m_x2 {}, m_y2 {}; // the window's upper-right corner (map coords)

    fixed_t min_x {}, min_y {}, max_x {}, max_y {}; // the level's map bounds
    fixed_t max_w {}, max_h {}; // max_x - min_x, max_y - min_y
    // No min_w/min_h: vanilla am_map.c assigned them 2*PLAYERRADIUS ("const? never
    // changed?", id's own comment) and read them nowhere - max_scale_mtof below is
    // computed from the literal, not from them. Verified against the 1993 source in
    // this repository's history; deleted rather than carried, as no read was lost.
    fixed_t min_scale_mtof {}; // zoom-out limit
    fixed_t max_scale_mtof {}; // zoom-in limit

    fixed_t old_m_w {}, old_m_h {}; // saved window for resize recovery
    fixed_t old_m_x {}, old_m_y {};
    MapPoint f_oldloc = {}; // the follower's previous location

    fixed_t scale_ftom {}; // frame -> map scale (1 / scale_mtof)

    Array<Patch*, 10> marknums = {}; // the 0-9 mark-number patches
    Array<MapPoint, numMarkPoints> markpoints = {}; // the placed marks
    int markpointnum = 0; // the next mark slot

    bool stopped = true; // the automap is closed
};

// The one AutomapView, a view onto the Engine's member - the same pattern as the other clusters
// (statusBarState(), hudState(), ...).
AutomapView& automapView();
} // namespace Doom
