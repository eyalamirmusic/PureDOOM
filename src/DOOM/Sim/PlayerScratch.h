#pragma once

#include "../doomtype.h" // doom_boolean

namespace Doom
{
// The player-movement scratch: onground, set once a tic by P_MovePlayer from whether the player is
// standing on the floor and read by the thrust code (no thrust while airborne) and A_* weapon
// bob. Its own one-flag cluster - Sim/Player keeps no other file-scope state.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); it was Sim/Player's
// own namespace-scope private global, read by no other file. movePlayer hoists playerScratch() once
// and reaches it through it; calcHeight and deathThink each touch it once and reach it inline,
// rather than through a file-scope reference alias (REFACTOR.md, Step 9 strand (a)). Live
// simulation-golden-covered - the demos walk every tic - so the byte-identical *.hashes are a live
// confirmation.
struct PlayerScratch
{
    doom_boolean onground = false; // the player is standing on the floor this tic
};

// The one PlayerScratch, a view onto the Engine's member (distinct from PlayerState, the roster).
PlayerScratch& playerScratch();
} // namespace Doom
