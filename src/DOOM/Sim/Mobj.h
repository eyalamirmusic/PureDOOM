#pragma once

#include "MobjTypes.h" // Mobj, MapThing, StateNum, MobjType

namespace Doom
{
// Moving-object handling: spawning, the per-tic mobj thinker, removal, missiles,
// puffs and blood. p_mobj.cpp keeps the vanilla names as shims - callers everywhere
// (and info.cpp's state actions, p_saveg, the sim probe) use those. Covered exactly
// by the demos (every monster, missile and item is an mobj) and golden-neutral.

bool setMobjState(Mobj& mobj, StateNum state);
void mobjThinker(Mobj& mobj);
Mobj* spawnMobj(fixed_t x, fixed_t y, fixed_t z, MobjType type);
void removeMobj(Mobj& mobj);
void respawnSpecials();
void spawnPlayer(MapThing& mthing);
void spawnMapThing(MapThing& mthing);
void spawnPuff(fixed_t x, fixed_t y, fixed_t z);
void spawnBlood(fixed_t x, fixed_t y, fixed_t z, int damage);
Mobj* spawnMissile(Mobj& source, Mobj* dest, MobjType type);
void spawnPlayerMissile(Mobj& source, MobjType type);
} // namespace Doom
