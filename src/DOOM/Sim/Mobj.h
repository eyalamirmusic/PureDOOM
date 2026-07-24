#pragma once

#include "MobjTypes.h" // Mobj, MapThing, StateNum, MobjType

namespace Doom
{
// Moving-object handling: spawning, the per-tic mobj thinker, removal, missiles,
// puffs and blood. p_mobj.cpp keeps the vanilla names as shims - callers everywhere
// (and info.cpp's state actions, p_saveg, the sim probe) use those. Covered exactly
// by the demos (every monster, missile and item is an mobj) and golden-neutral.

bool setMobjState(Mobj& mobj, StateNum state);

// The three steps of the mobj's per-tic thinker. Their bodies stay in Mobj.cpp
// (they reach deep into this file's movement/collision helpers), but Mobj::tick()
// - which drives them - lives in Thinkers/Mobj.cpp, so they are declared here for
// it to call. Nothing else calls them.
void xyMovement(Mobj& mo);
void zMovement(Mobj& mo);
void nightmareRespawn(Mobj& mobj);

Mobj* spawnMobj(Fixed x, Fixed y, Fixed z, MobjType type);
void removeMobj(Mobj& mobj);
void respawnSpecials();
void spawnPlayer(MapThing& mthing);
void spawnMapThing(MapThing& mthing);
void spawnPuff(Fixed x, Fixed y, Fixed z);
void spawnBlood(Fixed x, Fixed y, Fixed z, int damage);
Mobj* spawnMissile(Mobj& source, Mobj* dest, MobjType type);
void spawnPlayerMissile(Mobj& source, MobjType type);
} // namespace Doom
