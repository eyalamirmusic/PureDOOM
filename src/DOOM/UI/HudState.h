#pragma once

#include "../doomtype.h" // doom_boolean
#include "../hu_lib.h" // hu_textline_t

// player_t is used only by pointer here, so a forward declaration is enough.
struct player_t;

namespace Doom
{
// What is left of UI/Hud's own file-scope state once the message line and the chat moved out
// (HudMessage / HudChat): the console player the HUD reads from (plr, refreshed at Doom::startHud), the
// level-title text line drawn for a few seconds on entry (w_title), and headsupactive, the flag
// Doom::startHud/HU_Stop raise and drop so the HUD is set up exactly once per level. None is read by any
// other file.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); the vanilla names
// become references onto these members. The level title lands in screens[0] on entry, which the
// demos exercise every level start, so this is frame-golden-covered.
struct HudState
{
    player_t* plr = nullptr; // the console player the HUD reads from
    hu_textline_t w_title = {}; // the level-title text line, drawn on entry
    doom_boolean headsupactive = false; // the HUD is set up (gate against re-init)
};

// The one HudState, a view onto the Engine's member - the same pattern as the other clusters
// (hudMessage(), hudChat(), ...).
HudState& hudState();
} // namespace Doom
