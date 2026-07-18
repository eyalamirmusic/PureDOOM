#pragma once

#include "../doomdef.h" // GameState

namespace Doom
{
// Doom::displayFrame's frame-diff state: the previous frame's overlay/view flags and the border-redraw
// countdown it compares against to decide what to repaint. viewactivestate/menuactivestate/
// inhelpscreensstate/fullscreen latch last frame's viewactive/menuactive/inhelpscreens/full-view so
// a change forces a status-bar or border redraw; oldgamestate latches the last drawn gamestate (a
// change repaints the background and, at the finale/intermission boundaries, the palette);
// borderdrawcount counts down the three frames the view border must be redrawn over.
//
// Moved into the Engine by the file-scope-statics sweep's function-local pass (REFACTOR.md, Step 5).
// These were displayFrame's own function-local statics, read by no other function; vanilla never resets
// them (they persist for the process), so each becomes a member with a matching default reached by a
// local reference in displayFrame - identical persistence in a single-Engine process. displayFrame runs
// every tic in the headless suite, so this is live frame-golden-covered.
struct DisplayState
{
    doom_boolean viewactivestate = false; // last frame's viewactive
    doom_boolean menuactivestate = false; // last frame's menuactive
    doom_boolean inhelpscreensstate = false; // last frame's inhelpscreens
    doom_boolean fullscreen = false; // last frame's full-view (viewheight == 200)
    GameState oldgamestate =
        static_cast<GameState>(-1); // last drawn gamestate, -1 forces a redraw
    int borderdrawcount = 0; // frames of view-border redraw still owed
};

// The one DisplayState, a view onto the Engine's member - the same pattern as the other clusters
// (gameFlow(), refreshFlags(), ...).
DisplayState& displayState();
} // namespace Doom
