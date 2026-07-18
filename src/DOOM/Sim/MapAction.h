#pragma once

#include "../p_local.h" // mobj_t, player_t, angle_t, fixed_t, sector_t

namespace Doom
{
// The rest of vanilla p_map, past the movement core in Sim/Movement: sliding a
// blocked move along a wall, hitscan aiming and firing, using a line in front of
// the player, splash damage, and re-clipping things after a sector moves. p_map.cpp
// keeps the vanilla names (Doom::slideMove, Doom::lineAttack, ...) as shims forwarding here.
//
// The scratch these share with their P_PathTraverse / blockmap callbacks is
// file-local to MapAction.cpp (the callbacks are function pointers, so the state
// they read cannot be passed as arguments) - only the two results other files read,
// the aim's linetarget and the shot's attackrange, live in Clip.
//
// Covered by the demos (a level's worth of firing, sliding along walls, doors
// crushing) and golden-neutral: none of this scratch is hashed.

// Slide `mo` along the first wall its momentum would drive it into, so it grazes
// angled walls instead of stopping dead.
void slideMove(mobj_t* mo);

// Trace a shot from t1 at the given angle and find the auto-aim slope to the first
// shootable thing in view; sets Clip's linetarget. Returns the slope, or 0 if
// nothing was found.
fixed_t aimLineAttack(mobj_t* t1, angle_t angle, fixed_t distance);

// Fire a hitscan from t1 at angle/slope for `distance`, spawning puffs or blood and
// dealing `damage` (damage 0 is a test trace that only sets linetarget).
void lineAttack(
    mobj_t* t1, angle_t angle, fixed_t distance, fixed_t slope, int damage);

// Activate the special line the player is facing, within USERANGE.
void useLines(player_t* player);

// Damage every shootable thing within `damage` map units of spot that spot can see.
void radiusAttack(mobj_t* spot, mobj_t* source, int damage);

// After a sector changed height, re-clip every thing touching it; crush those that
// no longer fit if `crunch`. Returns true if anything did not fit.
bool changeSector(sector_t* sector, doom_boolean crunch);
} // namespace Doom
