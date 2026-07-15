#pragma once

#include "../m_fixed.h" // fixed_t

namespace Doom
{
// The movement-speed lookup tables G_BuildTiccmd applies to the player's input when building a
// ticcmd: forwardmove[speed] and sidemove[speed] are the walk/run forward and strafe amounts
// (indexed by whether the run key is held), and angleturn[tspeed] the per-tic turn amount (fast,
// faster, and a slow-turn value used for the first few tics of a key press). MAXPLMOVE is
// forwardmove[1], the run speed, which also caps the combined forward/side move.
//
// forwardmove/sidemove are not purely fixed reference data: D_DoomLoop's -turbo handling scales
// both at startup (forwardmove[i] = forwardmove[i] * scale / 100), so they are genuine per-session
// state. That write lives in Game/DoomMain.cpp, which is why forwardmove/sidemove are the only two
// of the three read outside g_game - their DoomMain externs move to references-to-array in lockstep
// with the definitions here (an untouched `extern int forwardmove[2]` against a reference member
// would silently read the reference's hidden pointer as the array). angleturn is g_game's alone.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); the vanilla names
// become references-to-array onto these members, so every indexed read is unchanged. G_BuildTiccmd
// is the only consumer, and in the headless suite the demo playback overrides the command it builds
// (and -turbo is never passed), so the move is golden-neutral.
struct MovementSpeeds
{
    fixed_t forwardmove[2] = {0x19, 0x32}; // walk, run
    fixed_t sidemove[2] = {0x18, 0x28}; // walk, run
    fixed_t angleturn[3] = {640, 1280, 320}; // fast, faster, + slow turn
};

// The one MovementSpeeds, a view onto the Engine's member - the same pattern as the other Game/
// clusters (ticcmdInput(), gameSession(), ...).
MovementSpeeds& movementSpeeds();
} // namespace Doom
