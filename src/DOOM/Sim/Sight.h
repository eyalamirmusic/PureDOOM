#pragma once

#include "SimDefs.h" // Mobj

namespace Doom
{
// Line-of-sight: is a straight line from t1's eyes to any part of t2 unobstructed?
// The REJECT lump rules out most pairs without a trace; the rest walk the BSP,
// narrowing a vertical slope window - local to this call, threaded by reference
// through the BSP walk - against each two-sided line the sight line crosses.
//
// p_sight.cpp keeps the vanilla name Doom::checkSight as a shim; p_enemy (monster AI)
// and MapAction (radius attack) call it. Golden-neutral - the sight scratch is never
// hashed - and covered by every demo with a monster that can see the player.
bool checkSight(Mobj* t1, Mobj* t2);
} // namespace Doom
