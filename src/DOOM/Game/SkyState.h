#pragma once

namespace Doom
{
// The lump number of the sky flat (F_SKY1). R_InitFlats resolves it once at startup, and it
// is then the sentinel that means "sky here, not a real flat": the renderer draws the sky
// cylinder instead of a ceiling wherever a sector's ceilingpic equals it, and the playsim
// lets a projectile vanish silently (no wall puff) when it strikes such a surface. doomstat.h's
// skyflatnum in "Internal parameters, used for engine".
//
// A cluster of doomstat.h's game state moved off the loose globals into the Engine
// (REFACTOR.md, Step 5). Externed only in doomstat.h and defined in r_sky.cpp (a flat renderer
// shim, no namespace); the vanilla name becomes a reference onto this member. Shared by the
// renderer, the playsim and the app's sky detection, all of which read it through the engine
// headers unchanged. A reference reads the identical value, so the move is golden-neutral.
struct SkyState
{
    int skyflatnum = 0; // lump number of the sky flat (F_SKY1)
};

// The one SkyState, a view onto the Engine's member - the same pattern as
// intermissionInfo(), ammoLimits(), gameClock(), mapSpawns(), netState() and overlayState().
SkyState& skyState();
} // namespace Doom
