#pragma once

#include "../doomtype.h" // doom_boolean

namespace Doom
{
// The demo subsystem's state: the flags for whether a demo is in play, and the buffer it plays
// from or records into. usergame is true once a real, human-driven game is running (so the menu
// allows save / end game); demoplayback is true while a recorded demo is driving the input
// instead; demorecording while one is being captured; singledemo is set by -playdemo so the
// process quits after the one demo rather than looping the title screen. demobuffer is the demo
// byte block, demo_p the read/write cursor walking it (G_ReadDemoTiccmd / G_WriteDemoTiccmd),
// demoend its end guard; demoname is the lump/file name -playdemo or -record was given, netdemo
// whether the demo in play was recorded over a netgame. doomstat.h's "DEMO playback/recording
// related stuff", plus g_game's own buffer state beside it.
//
// The four flags were a doomstat.h loose-global cluster; the five buffer fields are g_game's own
// file-scope state, read by no other file, folded in here as the file-scope-statics sweep
// reaches them - one demo-subsystem owner (REFACTOR.md, Step 5). All were defined in
// Game/Game.cpp above its namespace (a state owner); the vanilla names become references onto
// the members (demoname/the buffer pointers as references-to-array and reference-to-pointer).
// demorecording carries extra file-scope externs (Game/DoomMain.cpp, Host/System.cpp's fatalError
// demo flush), updated to references in step. None of the fields is hashed, so the move is
// golden-neutral - and the demo replays drive demobuffer/demo_p end to end, confirming it.
struct DemoState
{
    doom_boolean usergame = 0; // a real game is running; save / end allowed
    doom_boolean demoplayback = 0; // a recorded demo is driving input
    doom_boolean demorecording = 0; // a demo is being captured
    doom_boolean singledemo = 0; // -playdemo: quit after the one demo

    char demoname[32] = {}; // the demo lump/file name
    doom_boolean netdemo = 0; // the demo in play was recorded over a netgame
    byte* demobuffer = nullptr; // the demo byte block
    byte* demo_p = nullptr; // read/write cursor into demobuffer
    byte* demoend = nullptr; // end guard of demobuffer
};

// The one DemoState, a view onto the Engine's member - the same pattern as
// gameFlow(), playerState(), gameSession(), levelStats(), clip(), level() and wad().
DemoState& demoState();
} // namespace Doom
