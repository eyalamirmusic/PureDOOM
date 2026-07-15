#pragma once

#include "../d_player.h" // wbstartstruct_t

namespace Doom
{
// The parameters the intermission screen runs on. When a level ends, G_DoCompleted fills
// wminfo - the previous and next map, the par time, and each player's kill/item/secret
// tallies against the map totals - and WI_Start reads it to animate the between-levels
// scoreboard. doomstat.h's wminfo in "Internal parameters, fixed".
//
// A cluster of doomstat.h's game state moved off the loose globals into the Engine
// (REFACTOR.md, Step 5). Externed only in doomstat.h and defined in Game/Game.cpp above its
// namespace (a state owner); the vanilla name becomes a reference onto this member. No demo
// reaches the intermission, so it is not hashed - golden-neutral either way.
struct IntermissionInfo
{
    wbstartstruct_t wminfo =
        {}; // parameters for the world-map / intermission screen
};

// The one IntermissionInfo, a view onto the Engine's member - the same pattern as
// ammoLimits(), gameClock(), mapSpawns(), netState(), overlayState() and refreshFlags().
IntermissionInfo& intermissionInfo();
} // namespace Doom
