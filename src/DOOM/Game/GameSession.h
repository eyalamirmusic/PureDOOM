#pragma once

#include "../doomdef.h" // skill_t
#include "../doomtype.h" // doom_boolean

namespace Doom
{
// The current game's rules, chosen when a game is started and read across the playsim and
// the game loop: gameskill / gameepisode / gamemap are the skill and the map being played,
// respawnmonsters has the dead come back (nightmare, or -respawn), netgame is true once
// packets are being broadcast, and deathmatch true only for a net deathmatch. doomstat.h's
// "Selected skill type, map etc." section (the "Selected by user" half) plus the netgame
// and deathmatch flags beside it.
//
// A cluster of doomstat.h's game state moved off the loose globals into the Engine
// (REFACTOR.md, Step 5). All six were externed only in doomstat.h and defined in
// Game/Game.cpp above its namespace (a state owner); the vanilla names become references
// onto the members. respawnmonsters/netgame/deathmatch steer the simulation and so are
// indirectly on the hash's path, but a reference reads the identical value, so the move is
// golden-neutral.
struct GameSession
{
    skill_t gameskill = sk_baby; // the skill being played (vanilla zero-inits this)
    int gameepisode = 0; // the episode being played
    int gamemap = 0; // the map being played

    doom_boolean respawnmonsters = 0; // the dead come back (nightmare / -respawn)
    doom_boolean netgame = 0; // packets are being broadcast
    doom_boolean deathmatch = 0; // started as a net deathmatch
};

// The one GameSession, a view onto the Engine's member - the same pattern as
// gameVersion(), launchOptions(), levelStats(), clip(), level(), wad() and randomness().
GameSession& gameSession();
} // namespace Doom
