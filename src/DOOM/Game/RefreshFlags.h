#pragma once

#include "../doomtype.h" // doom_boolean

namespace Doom
{
// The refresh flags the game loop sets and the display reads. paused freezes the tic while
// still redrawing; viewactive is true when the 3D view is being drawn (false when the
// full-screen automap has replaced it); nodrawers and noblit are the -nodraw / -noblit timing
// switches that skip the render and the blit for a -timedemo benchmark. doomstat.h's "Status
// flags for refresh" (the ones the game loop owns; automapactive and menuactive live with
// their own subsystems, statusbaractive is long dead).
//
// A cluster of doomstat.h's game state moved off the loose globals into the Engine
// (REFACTOR.md, Step 5). All four were externed only in doomstat.h and defined in
// Game/Game.cpp above its namespace (a state owner); the vanilla names become references onto
// the members. None is hashed, so the move is golden-neutral. (viewactive was deliberately
// left out of the renderer's Step-5 clusters as game-loop-owned rather than renderer-owned;
// this is where it lands.)
struct RefreshFlags
{
    doom_boolean paused = 0; // tic frozen, still redrawing
    doom_boolean viewactive = 0; // the 3D view is being drawn
    doom_boolean nodrawers = 0; // -nodraw: skip the render (timing)
    doom_boolean noblit = 0; // -noblit: skip the blit (timing)
};

// The one RefreshFlags, a view onto the Engine's member - the same pattern as
// demoState(), gameFlow(), playerState(), gameSession(), levelStats(), level() and wad().
RefreshFlags& refreshFlags();
} // namespace Doom
