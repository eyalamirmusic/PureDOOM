#pragma once

#include "../doomdef.h" // gamestate_t

namespace Doom
{
// The top-level game state and the wipe that guards its transitions. gamestate is which
// screen the game is on - a level, the intermission, a finale, or the title/demo loop - read
// all over the loop, the HUD and the status bar to decide what to draw and what input means.
// wipegamestate is the state the last displayed frame belonged to; D_Display melts the
// screen whenever the two differ (and G_DoLoadLevel sets it to -1 to force one). doomstat.h's
// "gamestate" and the "wipegamestate can be set to -1" note.
//
// A cluster of doomstat.h's game state moved off the loose globals into the Engine
// (REFACTOR.md, Step 5). gamestate was defined in Game/Game.cpp and wipegamestate in
// Game/DoomMain.cpp, each above its namespace; the vanilla names become references onto
// these members. wipegamestate has extra file-scope externs (Game/Game.cpp, UI/Finale.cpp)
// and gamestate a function-body extern (Host/Api.cpp's crosshair draw); those are updated to
// references in step. gamestate is hashed by the probe, but a reference reads the identical
// value, so the move is golden-neutral.
struct GameFlow
{
    gamestate_t gamestate =
        GS_LEVEL; // which screen we are on (vanilla zero-inits this)
    gamestate_t wipegamestate =
        GS_DEMOSCREEN; // the last drawn frame's state; -1 forces a wipe
};

// The one GameFlow, a view onto the Engine's member - the same pattern as
// playerState(), gameSession(), gameVersion(), levelStats(), clip(), level() and wad().
GameFlow& gameFlow();
} // namespace Doom
