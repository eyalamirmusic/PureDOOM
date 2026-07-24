#pragma once

#include "Event.h" // GameAction
#include "GameDefs.h" // GameState

namespace Doom
{
// The top-level game state, the pending action that drives it, and the wipe that guards its
// transitions - the game-flow triple g_game owns. gameaction is the deferred action gameTicker
// dispatches at the top of each tic (load a level, start a game, run a demo, complete the
// level), cleared to GameAction::Nothing once run. gamestate is which screen the game is on - a level,
// the intermission, a finale, or the title/demo loop - read all over the loop, the HUD and the
// status bar to decide what to draw and what input means. wipegamestate is the state the last
// displayed frame belonged to; displayFrame melts the screen whenever the two differ (and
// G_DoLoadLevel sets it to -1 to force one). doomstat.h's "gamestate", d_event.h's "gameaction"
// and the "wipegamestate can be set to -1" note.
//
// A cluster of doomstat.h's game state moved off the loose globals into the Engine
// (REFACTOR.md, Step 5). gamestate was defined in Game/Game.cpp and wipegamestate in
// Game/DoomMain.cpp, each above its namespace; the vanilla names become references onto
// these members. wipegamestate has extra file-scope externs (Game/Game.cpp, UI/Finale.cpp)
// and gamestate a function-body extern (Host/Api.cpp's crosshair draw); those are updated to
// references in step. gamestate is hashed by the probe, but a reference reads the identical
// value, so the move is golden-neutral. gameaction (defined in Game/Game.cpp, externed only in
// d_event.h, read by the loop and UI/Finale) completes the triple - transient within a tic, so
// GameAction::Nothing at the hash point, and golden-neutral through the reference all the same.
struct GameFlow
{
    GameAction gameaction =
        GameAction::Nothing; // the deferred action gameTicker runs each tic
    GameState gamestate =
        GameState::Level; // which screen we are on (vanilla zero-inits this)
    GameState wipegamestate = GameState::
        DemoScreen; // the last drawn frame's state; GS_FORCE_WIPE forces a wipe

    // A screen melt is currently animating: displayFrame raises it, doom_update drains
    // it via updateWipe instead of doomLoop, and the eacp compositor reads it.
    bool is_wiping_screen = false;
};

// The one GameFlow, a view onto the Engine's member - the same pattern as
// playerState(), gameSession(), gameVersion(), levelStats(), clipping(), level() and wad().
GameFlow& gameFlow();
} // namespace Doom
