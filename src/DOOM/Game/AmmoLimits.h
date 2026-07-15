#pragma once

#include "../doomdef.h" // NUMAMMO

namespace Doom
{
// The maximum a player may carry of each ammo type. P_GiveAmmo clamps a pickup against these,
// and a backpack doubles them; the status bar and the HUD read them to show "have / max".
// doomstat.h's maxammo in "Internal parameters, fixed" (its comment: "This doubles with
// BackPack powerup item"). Distinct from p_inter's clipammo, the per-pickup amounts, which is
// not a doomstat global and stays.
//
// A cluster of doomstat.h's game state moved off the loose globals into the Engine
// (REFACTOR.md, Step 5). maxammo was declared in both doomstat.h and p_local.h and defined in
// p_inter.cpp (a flat playsim shim); the vanilla name becomes a reference-to-array onto this
// member, and both header externs move in step (an untouched one would reintroduce the old
// ::maxammo symbol). Reached by the playsim, so a reference reads the identical value and the
// move is golden-neutral.
struct AmmoLimits
{
    int maxammo[NUMAMMO] = {200, 50, 300, 50}; // carry cap per ammo type
};

// The one AmmoLimits, a view onto the Engine's member - the same pattern as
// gameClock(), mapSpawns(), netState(), overlayState(), refreshFlags() and demoState().
AmmoLimits& ammoLimits();
} // namespace Doom
