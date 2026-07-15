#pragma once

#include "../p_mobj.h" // mobj_t, mapthing_t, statenum_t, mobjtype_t

namespace Doom
{
// Moving-object handling: spawning, the per-tic mobj thinker, removal, missiles,
// puffs and blood. p_mobj.cpp keeps the vanilla names as shims - callers everywhere
// (and info.cpp's state actions, p_saveg, the sim probe) use those. Covered exactly
// by the demos (every monster, missile and item is an mobj) and golden-neutral.

doom_boolean setMobjState(mobj_t* mobj, statenum_t state);
void mobjThinker(mobj_t* mobj);
mobj_t* spawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type);
void removeMobj(mobj_t* mobj);
void respawnSpecials();
void spawnPlayer(mapthing_t* mthing);
void spawnMapThing(mapthing_t* mthing);
void spawnPuff(fixed_t x, fixed_t y, fixed_t z);
void spawnBlood(fixed_t x, fixed_t y, fixed_t z, int damage);
mobj_t* spawnMissile(mobj_t* source, mobj_t* dest, mobjtype_t type);
void spawnPlayerMissile(mobj_t* source, mobjtype_t type);
} // namespace Doom
