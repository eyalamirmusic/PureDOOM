#pragma once

#include "GameDefs.h" // MAXPLAYERS
#include "../Wad/MapFormat.h" // MapThing

#include <ea_data_structures/Structures/Array.h>

namespace Doom
{
// The deathmatch start table's size. Was doomstat.h.
constexpr int MAX_DM_STARTS = 10;

// Where players and deathmatch frags spawn on the current map. Doom::setupLevel walks the map's
// things and records each player-start into playerstarts (indexed by player number) and each
// deathmatch start into deathmatchstarts, with deathmatch_p the append cursor into that array.
// PureDOOM is single-player, so only playerstarts[0] is really used, but Doom::loadThings fills
// all three. doomstat.h's spawn-spot pair in "Internal parameters, fixed".
//
// A cluster of doomstat.h's game state moved off the loose globals into the Engine
// (REFACTOR.md, Step 5). All three were externed only in doomstat.h and defined in
// p_setup.cpp (a flat renderer/playsim shim, no namespace); the vanilla names become
// references onto the members, the arrays as references-to-array. deathmatchstarts is sized
// MAX_DM_STARTS here to match its doomstat.h extern (p_setup.cpp's own MAX_DEATHMATCH_STARTS
// is the same 10). Not hashed by the simulation probe (the demos are single-player and read
// only playerstarts[0]), so golden-neutral.
struct MapSpawns
{
    EA::Array<MapThing, MAX_DM_STARTS> deathmatchstarts =
        {}; // deathmatch frag spawns
    MapThing* deathmatch_p = nullptr; // append cursor into the above
    EA::Array<MapThing, MAXPLAYERS> playerstarts = {}; // per-player start spots
};

// The one MapSpawns, a view onto the Engine's member - the same pattern as
// netState(), overlayState(), refreshFlags(), demoState(), gameFlow() and playerState().
MapSpawns& mapSpawns();
} // namespace Doom
