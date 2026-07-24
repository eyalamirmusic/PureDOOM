#pragma once

#include "AutomapTypes.h" // MapPoint (and Doom::Fixed through it)
#include "../Game/GameDefs.h" // SCREENWIDTH, SCREENHEIGHT
#include "../doomtype.h" // byte
#include "../Sim/MapTypes.h"
#include "../Render/RenderTypes.h" // Patch

#include "../Containers.h"

namespace Doom
{
// The automap's scale on entry. Lives here rather than in UI/Automap.cpp because
// AutomapView::scale_mtof is initialised from it.
constexpr Fixed INITSCALEMTOF {(std::int32_t) (.2 * FRACUNIT.raw)};

// The whole of the automap's view state. The window (m_x/m_y/m_w/m_h, scale_mtof),
// the frame rect (f_x/f_y/f_w/f_h), the followed player and the two cheats used to
// be loose globals the am_map.cpp shim exported for the GPU automap to read; they
// are members here now, and examples/EACP reads them through automapView() like
// everything else. The rest was always here: the pan/zoom increments and limits,
// the level's map bounds, the saved window for resize recovery, the follower's old
// location, the frame->map scale, the placed marks, and the open/closed flag. (Named AutomapView, not AutomapState, because a StatusBarMode enum in
// UI/StatusBarTypes.h used to carry a namespace-scope value AutomapState that would have been
// ambiguous with a AutomapState under `using namespace Doom`. Scoping the enum ended the
// clash, and the enum itself has since been deleted as dead; the name here is kept only because
// nothing gains from renaming it.)
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
    int bigstate = 0; // automapResponder: the "big" (zoomed-out overview) toggle
    int nexttic = 0; // automapTicker: next tic the fuse animation advances
    int litelevelscnt = 0; // automapTicker: cursor into the fuse brightness ramp

    // Where the map is looking, and how far in. m_x/m_y is the lower-left corner in
    // map coordinates; scale_mtof converts a map distance to a frame one. m_w/m_h is
    // how much of the map the window spans. These four and scale_mtof used to be
    // loose globals in UI/AutomapTypes.h, exported for the GPU automap to read.
    Fixed m_x {}, m_y {};
    Fixed m_w {}, m_h {};
    Fixed scale_mtof = INITSCALEMTOF;

    // The map's rect within the frame.
    int f_x = 0, f_y = 0, f_w = 0, f_h = 0;

    // The player it draws the arrow for, and whether it is keeping them centred.
    Player* am_plr = nullptr;
    int followplayer = 1;

    // The map cheats: `cheating` reveals the walls and the things, `grid` the grid.
    int cheating = 0;
    int grid = 0;

    // Wall brightness, which the map strobes.
    int lightlev = 0;

    MapPoint m_paninc = {}; // window pan per tic (map coords)
    Fixed mtof_zoommul {}; // window zoom per tic (map -> frame)
    Fixed ftom_zoommul {}; // window zoom per tic (frame -> map)

    Fixed m_x2 {}, m_y2 {}; // the window's upper-right corner (map coords)

    Fixed min_x {}, min_y {}, max_x {}, max_y {}; // the level's map bounds
    Fixed max_w {}, max_h {}; // max_x - min_x, max_y - min_y
    // No min_w/min_h: vanilla am_map.c assigned them 2*PLAYERRADIUS ("const? never
    // changed?", id's own comment) and read them nowhere - max_scale_mtof below is
    // computed from the literal, not from them. Verified against the 1993 source in
    // this repository's history; deleted rather than carried, as no read was lost.
    Fixed min_scale_mtof {}; // zoom-out limit
    Fixed max_scale_mtof {}; // zoom-in limit

    Fixed old_m_w {}, old_m_h {}; // saved window for resize recovery
    Fixed old_m_x {}, old_m_y {};
    MapPoint f_oldloc = {}; // the follower's previous location

    Fixed scale_ftom {}; // frame -> map scale (1 / scale_mtof)

    Array<Patch*, 10> marknums = {}; // the 0-9 mark-number patches
    Array<MapPoint, numMarkPoints> markpoints = {}; // the placed marks
    int markpointnum = 0; // the next mark slot

    bool stopped = true; // the automap is closed
};

// The one AutomapView, a view onto the Engine's member - the same pattern as the other clusters
// (statusBarState(), hudState(), ...).
AutomapView& automapView();
} // namespace Doom
