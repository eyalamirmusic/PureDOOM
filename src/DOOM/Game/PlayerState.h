#pragma once

#include "PlayerTypes.h" // Player
#include "../doomdef.h" // MAXPLAYERS
#include "../doomtype.h" // doom_boolean

namespace Doom
{
// The player roster and which of them this node cares about. players is the array of every
// player's state (only single-player is populated here); playeringame marks which slots are
// live; consoleplayer is the one this node takes input for; displayplayer the one whose view
// is drawn. doomstat.h scatters these across its "Status flags" (the two indices) and
// "Internal parameters, fixed" (the two arrays) sections; they are one concern.
//
// A cluster of doomstat.h's game state moved off the loose globals into the Engine
// (REFACTOR.md, Step 5). All four were externed only in doomstat.h and defined in
// Game/Game.cpp above its namespace (a state owner); the vanilla names become references
// onto the members - the two arrays as references-to-array, so the type and every indexed
// read (including vanilla's own (int*) casts into playeringame, which rely on doom_boolean
// being int-sized) are unchanged. players' fields are hashed by the simulation probe, but a
// reference reads the identical bytes, so the move is golden-neutral.
struct PlayerState
{
    Player players[MAXPLAYERS] = {}; // every player's state (single-player here)
    doom_boolean playeringame[MAXPLAYERS] = {}; // which slots are live

    int consoleplayer = 0; // the player this node takes events for
    int displayplayer = 0; // the player whose view is drawn
};

// The one PlayerState, a view onto the Engine's member - the same pattern as
// gameSession(), gameVersion(), launchOptions(), levelStats(), clip(), level() and wad().
PlayerState& playerState();
} // namespace Doom
