#pragma once

#include "../doomtype.h" // doom_boolean

namespace Doom
{
// Whether a demo is in play and whether the game may be saved. usergame is true once a real,
// human-driven game is running (so the menu allows save / end game); demoplayback is true
// while a recorded demo is driving the input instead; demorecording while one is being
// captured; singledemo is set by -playdemo so the process quits after the one demo rather
// than looping the title screen. doomstat.h's "DEMO playback/recording related stuff".
//
// A cluster of doomstat.h's game state moved off the loose globals into the Engine
// (REFACTOR.md, Step 5). All four were defined in Game/Game.cpp above its namespace (a state
// owner); the vanilla names become references onto the members. demorecording carries extra
// file-scope externs (Game/DoomMain.cpp, Host/System.cpp's I_Error demo flush), updated to
// references in step. None is hashed, so the move is golden-neutral.
struct DemoState
{
    doom_boolean usergame = 0; // a real game is running; save / end allowed
    doom_boolean demoplayback = 0; // a recorded demo is driving input
    doom_boolean demorecording = 0; // a demo is being captured
    doom_boolean singledemo = 0; // -playdemo: quit after the one demo
};

// The one DemoState, a view onto the Engine's member - the same pattern as
// gameFlow(), playerState(), gameSession(), levelStats(), clip(), level() and wad().
DemoState& demoState();
} // namespace Doom
