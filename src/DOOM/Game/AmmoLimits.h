#pragma once

#include "GameDefs.h" // NUMAMMO

#include "../Containers.h"

namespace Doom
{
// The two ammo tables, indexed by ammo type. maxammo is the carry cap P_GiveAmmo clamps a
// pickup against (a backpack doubles it; the status bar and HUD show "have / max"), and
// clipammo the base amount one pickup of that type gives (a dropped clip gives half, a box
// five times). maxammo is doomstat.h's ("This doubles with BackPack powerup item"); clipammo
// is p_local.h's, its natural sibling - together they are "the ammo tables".
//
// Moved off the loose globals into the Engine (REFACTOR.md, Step 5). maxammo was declared in
// both doomstat.h and p_local.h, clipammo in p_local.h; both were defined in p_inter.cpp (a
// flat playsim shim) and become references-to-array onto these members, every header extern
// moving in step (an untouched one would reintroduce the old :: symbol). Both are read on
// every ammo pickup the demos make, so a reference reads the identical value and the move is
// golden-neutral.
struct AmmoLimits
{
    Array<int, NUMAMMO> maxammo = {200, 50, 300, 50}; // carry cap per ammo type
    Array<int, NUMAMMO> clipammo = {10, 4, 20, 1}; // base amount one pickup gives
};

// The one AmmoLimits, a view onto the Engine's member - the same pattern as
// gameClock(), mapSpawns(), netState(), overlayState(), refreshFlags() and demoState().
AmmoLimits& ammoLimits();
} // namespace Doom
