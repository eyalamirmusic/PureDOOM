#pragma once

#include "../doomtype.h" // byte

#include "../Containers.h"

#include <string>

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
//
// demobuffer has dual ownership (REFACTOR.md, Step 9 strand (b)): recordDemo() allocates it to
// write a demo out, but doPlayDemo() instead points it at Doom::cacheLumpName's return - memory
// WadFile owns for the run of the process - to read one back. demobuffer can therefore not
// become an owning vector itself: doing so would make checkDemoStatus's demorecording cleanup
// free WAD lump memory on every demo playback. demoRecordBuffer is the RAII owner for the
// recording case only; demobuffer stays a raw VIEW pointer that points at
// demoRecordBuffer.data() while recording, or at the borrowed lump while playing back, and is
// never freed directly either way.
struct DemoState
{
    bool usergame = false; // a real game is running; save / end allowed
    bool demoplayback = false; // a recorded demo is driving input
    bool demorecording = false; // a demo is being captured
    bool singledemo = false; // -playdemo: quit after the one demo

    std::string demoname; // the demo lump/file name
    bool netdemo = false; // the demo in play was recorded over a netgame
    Vector<byte>
        demoRecordBuffer; // owns the buffer while recordDemo() is writing one
    byte* demobuffer = nullptr; // the demo byte block: a view, see the note above
    byte* demo_p = nullptr; // read/write cursor into demobuffer
    byte* demoend = nullptr; // end guard of demobuffer
};

// The one DemoState, a view onto the Engine's member - the same pattern as
// gameFlow(), playerState(), gameSession(), levelStats(), clip(), level() and wad().
DemoState& demoState();
} // namespace Doom
