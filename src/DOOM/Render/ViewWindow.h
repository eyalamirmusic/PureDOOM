#pragma once

namespace Doom
{
// The region of the screen the 3D view fills. R_SetViewSize / R_ExecuteSetViewSize
// derive it from the menu's screen-size choice: the view's width and height in pixels,
// the width before the low-detail halving (scaledviewwidth), and the top-left corner
// it is drawn from (viewwindowx / viewwindowy). The renderer sizes its column and span
// tables from it, the status bar and HUD position against it, and the app queries it to
// know whether the view fills the screen.
//
// The third scalar cluster off the loose globals into the Engine (REFACTOR.md, Step 5).
// Its globals were externed redundantly across doomstat.h, r_main.h and r_state.h and
// defined in r_draw.cpp; the definitions are now references onto these members and all
// three headers' externs are references onto them, so every reader resolves unchanged.
// (The engine global viewheight is unrelated to player_t::viewheight, the player's eye
// height - a separate field the playsim reads and this does not touch.) Nothing here is
// hashed, so gathering it is golden-neutral, as the earlier clusters were.
struct ViewWindow
{
    int viewwidth = 0;
    int viewheight = 0;
    int scaledviewwidth = 0;
    int viewwindowx = 0;
    int viewwindowy = 0;
};

// The one ViewWindow, a view onto the Engine's member - the same pattern as
// viewProjection(), viewPoint(), clip(), level(), wad() and randomness().
ViewWindow& viewWindow();
} // namespace Doom
