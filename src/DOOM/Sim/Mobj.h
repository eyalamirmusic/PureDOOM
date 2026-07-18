#pragma once

#include "../p_mobj.h" // Mobj, mapthing_t, statenum_t, mobjtype_t

namespace Doom
{
// Moving-object handling: spawning, the per-tic mobj thinker, removal, missiles,
// puffs and blood. p_mobj.cpp keeps the vanilla names as shims - callers everywhere
// (and info.cpp's state actions, p_saveg, the sim probe) use those. Covered exactly
// by the demos (every monster, missile and item is an mobj) and golden-neutral.

doom_boolean setMobjState(Mobj* mobj, statenum_t state);
void mobjThinker(Mobj* mobj);
Mobj* spawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type);
void removeMobj(Mobj* mobj);
void respawnSpecials();
void spawnPlayer(mapthing_t* mthing);
void spawnMapThing(mapthing_t* mthing);
void spawnPuff(fixed_t x, fixed_t y, fixed_t z);
void spawnBlood(fixed_t x, fixed_t y, fixed_t z, int damage);
Mobj* spawnMissile(Mobj* source, Mobj* dest, mobjtype_t type);
void spawnPlayerMissile(Mobj* source, mobjtype_t type);
} // namespace Doom
