#pragma once

#include "../doomtype.h" // doom_boolean

namespace Doom
{
// The two cross-read HUD flags the hu_stuff shim owns and other files read: chat_on (the chat
// input line is open, so Doom::gameResponder should route keys to it) and message_dontfuckwithme (a
// forced message is showing that a lower-priority one may not overwrite). Distinct from HudState,
// which holds UI/Hud's file-local four (plr, w_title, headsupactive) that no other file reads.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5). Externed in
// UI/Hud and UI/Menu, and written by Doom::hudTicker/Doom::hudResponder, so their hu_stuff.cpp definitions
// and every extern become references onto these members (a plain extern that wrote one would
// clobber the reference's storage). Live golden-covered - the HUD draws every tic.
struct HudFlags
{
    doom_boolean chat_on = false;                // the chat input line is open
    doom_boolean message_dontfuckwithme = false; // a forced message locks the line
};

HudFlags& hudFlags();
} // namespace Doom
