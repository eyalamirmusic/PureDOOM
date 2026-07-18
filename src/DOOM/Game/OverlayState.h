#pragma once

#include "../doomtype.h" // doom_boolean

namespace Doom
{
// Whether an interactive overlay is up over the world. automapactive is true while the
// full-screen automap has replaced the 3D view; menuactive while the menu is open. Both
// change what the frame shows and what a keypress means, and the loop, the HUD and the
// crosshair draw read them together (Doom::displayFrame, Host/Api's crosshair test). doomstat.h's
// "Status flags for refresh" overlay pair.
//
// A cluster of doomstat.h's game state moved off the loose globals into the Engine
// (REFACTOR.md, Step 5). automapactive was defined in am_map.cpp (a flat renderer shim) and
// menuactive in UI/Menu.cpp above its namespace; the vanilla names become references onto
// these members. Both carry extra externs updated in step - automapactive in UI/HudWidgets,
// UI/Hud and Host/Api (a function-body extern in the crosshair draw), menuactive in Host/Api
// beside it. Neither is hashed by the simulation probe (no demo opens a menu or the automap),
// so the move is golden-neutral.
struct OverlayState
{
    doom_boolean automapactive = 0; // the automap has replaced the view
    doom_boolean menuactive = 0; // the menu is open over the view
    doom_boolean inhelpscreens = 0; // a full-screen help page is showing (Doom::displayFrame
                                    // forces a border redraw when it clears)
};

// The one OverlayState, a view onto the Engine's member - the same pattern as
// refreshFlags(), demoState(), gameFlow(), playerState(), gameSession() and levelStats().
OverlayState& overlayState();
} // namespace Doom
