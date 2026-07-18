#pragma once

#include "../am_map.h" // mpoint_t (and fixed_t through it)
#include "../doomdef.h" // SCREENWIDTH, SCREENHEIGHT
#include "../doomtype.h" // byte, doom_boolean
#include "../r_defs.h" // Patch

namespace Doom
{
// The automap's internal view state - what UI/Automap keeps for itself, distinct from the handful
// of globals the am_map.cpp shim exports for the GPU automap to read (m_x/m_y/m_w/m_h, scale_mtof,
// f_x/f_y/f_w/f_h, the player/thing shapes, followplayer/grid/cheating). This is the rest: the
// pan/zoom increments and limits, the level's map bounds, the saved window for resize recovery, the
// follower's old location, the frame->map scale, the placed marks, and the open/closed flag. None
// is read by any other file. (Named AutomapView, not AutomapState, because st_stuff.h already has a
// global enum value AutomapState - the st_stateenum_t case - which would be ambiguous with a
// Doom::AutomapState under `using namespace Doom`.)
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); the vanilla names
// become references onto these members (the arrays as references-to-array). The automap is not
// frame-golden-covered (no demo opens it), but a reference alias is pure storage relocation - the
// compiler binds each name to its same-named member - so this is behaviour-preserving by
// construction, and verified by build + app-link. AM_LevelInit / AM_initVariables reset the view
// before it is drawn, so the defaults matter little, but they reproduce vanilla's initializers.
// (The "iddt" cheat sequence stays a file-local static: its cheatseq_t holds a pointer to its own
// byte array, which does not survive being a copyable struct member; it is an m_cheat concern.)
struct AutomapView
{
    static constexpr int numMarkPoints = 10; // AM_NUMMARKPOINTS in UI/Automap

    int leveljuststarted = 1; // kluge until AM_LevelInit runs
    int finit_width = SCREENWIDTH; // the automap frame's width
    int finit_height = SCREENHEIGHT - 32; // ... and height (above the status bar)

    byte* fb = nullptr; // the pseudo-framebuffer the automap draws into
    int amclock = 0; // the automap's own tic clock

    // Animation / level-change detection, folded in from function-local statics:
    int lastlevel = -1; // amStart: last map, to re-init on change
    int lastepisode = -1; // amStart: last episode
    int bigstate = 0; // Doom::automapResponder: the "big" (zoomed-out overview) toggle
    int nexttic = 0; // Doom::automapTicker: next tic the fuse animation advances
    int litelevelscnt = 0; // Doom::automapTicker: cursor into the fuse brightness ramp

    mpoint_t m_paninc = {}; // window pan per tic (map coords)
    fixed_t mtof_zoommul = 0; // window zoom per tic (map -> frame)
    fixed_t ftom_zoommul = 0; // window zoom per tic (frame -> map)

    fixed_t m_x2 = 0, m_y2 = 0; // the window's upper-right corner (map coords)

    fixed_t min_x = 0, min_y = 0, max_x = 0, max_y = 0; // the level's map bounds
    fixed_t max_w = 0, max_h = 0; // max_x - min_x, max_y - min_y
    fixed_t min_w = 0, min_h = 0; // smallest window (based on player size)
    fixed_t min_scale_mtof = 0; // zoom-out limit
    fixed_t max_scale_mtof = 0; // zoom-in limit

    fixed_t old_m_w = 0, old_m_h = 0; // saved window for resize recovery
    fixed_t old_m_x = 0, old_m_y = 0;
    mpoint_t f_oldloc = {}; // the follower's previous location

    fixed_t scale_ftom = 0; // frame -> map scale (1 / scale_mtof)

    Patch* marknums[10] = {}; // the 0-9 mark-number patches
    mpoint_t markpoints[numMarkPoints] = {}; // the placed marks
    int markpointnum = 0; // the next mark slot

    doom_boolean stopped = true; // the automap is closed
};

// The one AutomapView, a view onto the Engine's member - the same pattern as the other clusters
// (statusBarState(), hudState(), ...).
AutomapView& automapView();
} // namespace Doom
