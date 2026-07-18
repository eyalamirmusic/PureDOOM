#pragma once

#include "../doomtype.h" // doom_boolean

namespace Doom
{
// The attract-mode demo loop - what the game shows when no one is playing. With no game
// running, Doom::doAdvanceDemo walks demosequence through a fixed cycle (title, a recorded demo,
// credits, another demo, ...), and for each entry either plays a demo or displays a full-screen
// page: pagename is the lump of the page graphic and pagetic the tics it lingers before the
// loop advances. advancedemo is the request flag - Doom::advanceDemo raises it, and Doom::displayFrame runs
// Doom::doAdvanceDemo on the next tic - so the advance happens between tics rather than mid-draw.
// d_main.h's advancedemo and d_main's own demo-loop state.
//
// A d_main cluster moved off the loose globals into the Engine (REFACTOR.md, Step 5). All four
// were defined in Game/DoomMain.cpp; the vanilla names become references onto these members.
// advancedemo has cross-file externs (d_main.h, and a file-scope one in Game/Net.cpp's
// Doom::netUpdate) and pagename one in Game/Game.cpp's page drawer; those move to references in step.
// None is hashed - it is title/demo-loop display state - and the reference bindings are
// mechanical, so the move is golden-neutral, which the attract-mode demo replays confirm.
struct AttractMode
{
    doom_boolean advancedemo = false; // request to advance the loop next tic
    int demosequence = 0; // which entry of the attract cycle we are on
    int pagetic = 0; // tics the current page lingers
    const char* pagename = nullptr; // lump name of the current page graphic
};

// The one AttractMode, a view onto the Engine's member - the same pattern as the other
// Game/ clusters (gameFlow(), demoState(), ...).
AttractMode& attractMode();
} // namespace Doom
