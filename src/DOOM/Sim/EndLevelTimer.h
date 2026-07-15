#pragma once

#include "../doomtype.h" // doom_boolean

namespace Doom
{
// The deathmatch end-level timer (the -timer / -avg options). When P_SpawnSpecials sees -avg
// it arms a 20-minute countdown, or -timer <minutes> an arbitrary one; either way only in
// deathmatch. levelTimer is whether the countdown is running, levelTimeCount the tics left -
// P_UpdateSpecials decrements it each tic and calls G_ExitLevel when it reaches zero. p_spec.h's
// "End-level timer (-TIMER option)".
//
// A p_spec cluster moved off the loose globals into the Engine (REFACTOR.md, Step 5). Both were
// externed in p_spec.h and defined in the flat p_spec.cpp shim, which still owns the vanilla
// names - now references onto these members. The demos are single-player, so the timer never
// arms (nor is it hashed), but P_SpawnSpecials sets levelTimer = false on every level load, on
// the path the demos take, and the reference bindings are mechanical - so the move is
// golden-neutral.
struct EndLevelTimer
{
    doom_boolean levelTimer = false; // is the -timer/-avg countdown running
    int levelTimeCount = 0; // tics remaining until G_ExitLevel
};

// The one EndLevelTimer, a view onto the Engine's member - the same pattern as
// activeSpecials(), itemRespawnQueue() and the other Sim/ clusters.
EndLevelTimer& endLevelTimer();
} // namespace Doom
