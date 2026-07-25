#pragma once

#include "AutomapTypes.h" // MapPoint (and Fixed through it)
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

    // Where the map is looking, and how far in. m_origin is the lower-left corner
    // in map coordinates and m_size how much of the map the window spans;
    // scale_mtof converts a map distance to a frame one. These and scale_mtof used
    // to be loose globals in UI/AutomapTypes.h, exported for the GPU automap to
    // read.
    MapPoint m_origin {};
    MapPoint m_size {};
    Fixed scale_mtof = INITSCALEMTOF;

    // The map's rect within the frame, in whole pixels.
    Vec2i f_origin {};
    Vec2i f_size {};

    // The player it draws the arrow for, and whether it is keeping them centred.
    Player* am_plr = nullptr;
    int followplayer = 1;

    // The map cheats: `cheating` reveals the walls and the things, `grid` the grid.
    int cheating = 0;
    int grid = 0;

    // Wall brightness, added to every map colour. Vanilla strobed it from an
    // updateLightLev the shipping automapTicker never called, so it is pinned at 0
    // and the ramp that drove it is gone; the offset itself stays because both this
    // automap and the port's add it at every drawMline.
    int lightlev = 0;

    MapPoint m_paninc = {}; // window pan per tic (map coords)
    Fixed mtof_zoommul {}; // window zoom per tic (map -> frame)
    Fixed ftom_zoommul {}; // window zoom per tic (frame -> map)

    MapPoint m_far {}; // the window's upper-right corner (map coords)

    MapPoint minBound {}, maxBound {}; // the level's map bounds
    MapPoint maxSize {}; // maxBound - minBound
    // No min_w/min_h: vanilla am_map.c assigned them 2*PLAYERRADIUS ("const? never
    // changed?", id's own comment) and read them nowhere - max_scale_mtof below is
    // computed from the literal, not from them. Verified against the 1993 source in
    // this repository's history; deleted rather than carried, as no read was lost.
    Fixed min_scale_mtof {}; // zoom-out limit
    Fixed max_scale_mtof {}; // zoom-in limit

    MapPoint old_m_size {}; // saved window for resize recovery
    MapPoint old_m_origin {};
    MapPoint f_oldloc = {}; // the follower's previous location

    Fixed scale_ftom {}; // frame -> map scale (1 / scale_mtof)

    Array<Patch*, 10> marknums = {}; // the 0-9 mark-number patches
    Array<MapPoint, numMarkPoints> markpoints = {}; // the placed marks
    int markpointnum = 0; // the next mark slot

    bool stopped = true; // the automap is closed

    // The window's own coordinate transform, both directions. These read nothing
    // but this view's scale and rect, so they are methods rather than the file-local
    // helpers in UI/Automap.cpp they used to be - three of which reached back through
    // automapView() to find the very object they were transforming for. Kept inline
    // here because the automap calls them per map line.
    Fixed frameToMap(int x) const { return FixedMul(Fixed::fromInt(x), scale_ftom); }

    int mapToFrame(Fixed x) const { return FixedMul(x, scale_mtof).toInt(); }

    int mapXToFrame(Fixed x) const
    {
        return f_origin.x + mapToFrame(x - m_origin.x);
    }

    int mapYToFrame(Fixed y) const
    {
        return f_origin.y + (f_size.y - mapToFrame(y - m_origin.y));
    }
};

// The one AutomapView, a view onto the Engine's member - the same pattern as the other clusters
// (statusBarState(), hudState(), ...).
AutomapView& automapView();
} // namespace Doom
