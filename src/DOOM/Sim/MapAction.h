#pragma once

#include "SimDefs.h" // Mobj, Player, Doom::Angle, Doom::Fixed, Sector

namespace Doom
{
// The rest of vanilla p_map, past the movement core in Sim/Movement, is methods now:
// Mobj::slideMove (slide a blocked move along a wall), Mobj::lineAttack (fire a
// hitscan), Mobj::radiusAttack (splash damage), Player::useLines (use a line in
// front of the player) and Sector::changeSector (re-clip things after the sector
// moves) - declared on their structs, defined in Sim/MapAction.cpp. What stays a
// free function is aimLineAttack, whose t1 is a nullable pointer (fire() and bfgSpray
// pass a mobj's .target unguarded), and the P_PathTraverse / blockmap callbacks the
// iterator takes the address of.
//
// The scratch these share with their callbacks is file-local to MapAction.cpp,
// carried by capturing lambdas. Covered by the demos (a level's worth of firing,
// sliding along walls, doors crushing) and golden-neutral: none of this scratch is
// hashed.

// aimLineAttack's result: the slope to the target it found, and the target itself.
// `slope` is vanilla's 0-if-none; because 0 is also a genuine horizontal shot,
// `target` is the real "found" flag and callers must test it, not the slope.
struct AimResult
{
    Fixed slope {};
    Mobj* target = nullptr;
};

// Trace a shot from t1 at the given angle and find the auto-aim slope to the first
// shootable thing in view. AimResult::slope is 0 if nothing was found, which is
// ambiguous with a genuine horizontal shot - test AimResult::target instead. t1 is a
// pointer because it may be null (a mobj's .target passed unguarded).
AimResult aimLineAttack(Mobj* t1, Angle angle, Fixed distance);
} // namespace Doom
