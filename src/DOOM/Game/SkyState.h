#pragma once

namespace Doom
{
// The lump number of the sky flat (F_SKY1). Doom::initFlats resolves it once at startup, and it
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
//
// The two render-side sky parameters join it: skytexture (the wall texture G_DoLoadLevel picks
// for the current map's sky, read by the renderer and the app's GPU sky) and skytexturemid (the
// vertical offset Doom::initSkyMap sets it to be drawn at). All three are "the sky" and move together.
struct SkyState
{
    int skyflatnum = 0;    // lump number of the sky flat (F_SKY1)
    int skytexture = 0;    // texture number of the current map's sky
    int skytexturemid = 0; // vertical offset the sky is drawn at
};

// The one SkyState, a view onto the Engine's member - the same pattern as
// intermissionInfo(), ammoLimits(), gameClock(), mapSpawns(), netState() and overlayState().
SkyState& skyState();
} // namespace Doom
