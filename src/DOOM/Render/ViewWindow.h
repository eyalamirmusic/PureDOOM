#pragma once

#include "../doomtype.h" // doom_boolean

namespace Doom
{
// How big the 3D view is and where it sits. The menu asks for a size with Doom::setViewSize,
// which stashes the request (setsizeneeded raised, setblocks the screen-blocks it wants)
// and defers the work; Doom::executeSetViewSize then derives the region the view fills - its
// width and height in pixels, the width before the low-detail halving (scaledviewwidth),
// the top-left corner it is drawn from (viewwindowx / viewwindowy) - and the applied
// detail shift (0 = high, 1 = low). The renderer sizes its column and span tables from
// it, the status bar and HUD position against it, and the app queries it to know whether
// the view fills the screen. (setdetail, the requested detail, stays file-local to
// Render/Main - Doom::setViewSize and Doom::executeSetViewSize are the only code that touches it.)
//
// The third scalar cluster off the loose globals into the Engine (REFACTOR.md, Step 5),
// later extended with the sizing request (setsizeneeded / setblocks / detailshift). Its
// globals were externed redundantly across doomstat.h, r_main.h and r_state.h (the
// request pair via file-scope externs in Render/Main, Game and Host) and defined in
// r_draw.cpp / r_main.cpp; the definitions are now references onto these members and
// every extern is a reference onto them, so every reader resolves unchanged. (The engine
// global viewheight is unrelated to Player::viewheight, the player's eye height - a
// separate field the playsim reads and this does not touch.) Nothing here is hashed, so
// gathering it is golden-neutral, as the earlier clusters were.
struct ViewWindow
{
    int viewwidth = 0;
    int viewheight = 0;
    int scaledviewwidth = 0;
    int viewwindowx = 0;
    int viewwindowy = 0;

    // The pending sizing request Doom::setViewSize stashes for Doom::executeSetViewSize.
    doom_boolean setsizeneeded = false;
    int setblocks = 0;

    // The applied detail shift (0 = high, 1 = low), read by the drawers and the app.
    int detailshift = 0;
};

// The one ViewWindow, a view onto the Engine's member - the same pattern as
// viewProjection(), viewPoint(), clip(), level(), wad() and randomness().
ViewWindow& viewWindow();
} // namespace Doom
