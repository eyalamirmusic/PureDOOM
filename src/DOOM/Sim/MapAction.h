#pragma once

#include "SimDefs.h" // Mobj, Player, angle_t, fixed_t, Sector

namespace Doom
{
// The rest of vanilla p_map, past the movement core in Sim/Movement: sliding a
// blocked move along a wall, hitscan aiming and firing, using a line in front of
// the player, splash damage, and re-clipping things after a sector moves. p_map.cpp
// keeps the vanilla names (Doom::slideMove, Doom::lineAttack, ...) as shims forwarding here.
//
// The scratch these share with their P_PathTraverse / blockmap callbacks is
// file-local to MapAction.cpp, carried by capturing lambdas rather than globals
// (REFACTOR.md, Step 9 strand (a)) - except slideMove's, threaded through
// PTR_SlideTraverse's bare function pointer, which still lives in ActionScratch.
// The shot's attackrange is the one piece of attack scratch another file reads
// (Sim/Mobj.cpp's spawnPuff, from inside the traversal's own call stack) and so
// stays on Clip - see the comment there for why it is an input, not a result.
//
// Covered by the demos (a level's worth of firing, sliding along walls, doors
// crushing) and golden-neutral: none of this scratch is hashed.

// aimLineAttack's result: the slope to the target it found, and the target itself.
// `slope` is vanilla's 0-if-none; because 0 is also a genuine horizontal shot,
// `target` is the real "found" flag and callers must test it, not the slope.
struct AimResult
{
    fixed_t slope {};
    Mobj* target = nullptr;
};

// Slide `mo` along the first wall its momentum would drive it into, so it grazes
// angled walls instead of stopping dead.
void slideMove(Mobj* mo);

// Trace a shot from t1 at the given angle and find the auto-aim slope to the first
// shootable thing in view. AimResult::slope is 0 if nothing was found, which is
// ambiguous with a genuine horizontal shot - test AimResult::target instead.
AimResult aimLineAttack(Mobj* t1, angle_t angle, fixed_t distance);

// Fire a hitscan from t1 at angle/slope for `distance`, spawning puffs or blood and
// dealing `damage` (damage 0 is a test trace used only to find an aim target).
void lineAttack(
    Mobj* t1, angle_t angle, fixed_t distance, fixed_t slope, int damage);

// Activate the special line the player is facing, within USERANGE.
void useLines(Player* player);

// Damage every shootable thing within `damage` map units of spot that spot can see.
void radiusAttack(Mobj* spot, Mobj* source, int damage);

// After a sector changed height, re-clip every thing touching it; crush those that
// no longer fit if `crunch`. Returns true if anything did not fit.
bool changeSector(Sector* sector, bool crunch);
} // namespace Doom
