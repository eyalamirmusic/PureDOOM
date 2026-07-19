#pragma once

#include "GameDefs.h" // Skill
#include "../doomtype.h" // doom_boolean

namespace Doom
{
// The current game's rules, chosen when a game is started and read across the playsim and
// the game loop: gameskill / gameepisode / gamemap are the skill and the map being played,
// respawnmonsters has the dead come back (nightmare, or -respawn), netgame is true once
// packets are being broadcast, and deathmatch selects the free-for-all rules (see the
// tri-state note on the member - it is not a boolean). doomstat.h's
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
    Skill gameskill = sk_baby; // the skill being played (vanilla zero-inits this)
    int gameepisode = 0; // the episode being played
    int gamemap = 0; // the map being played

    doom_boolean respawnmonsters = 0; // the dead come back (nightmare / -respawn)
    doom_boolean netgame = 0; // packets are being broadcast

    // TRI-STATE, not a boolean: 0 coop, 1 deathmatch, 2 altdeath. DoomMain assigns 2
    // for -altdeath, and Sim/Interaction and Sim/Mobj gate the altdeath-only item
    // rules on `!= 2` while Game/Net packs it as a two-bit field. A bool collapses 1
    // and 2, silently turning altdeath's rules on for plain deathmatch - and no
    // golden would catch it, all three demos being single-player with deathmatch 0.
    int deathmatch = 0; // 0 coop, 1 deathmatch, 2 altdeath
};

// The one GameSession, a view onto the Engine's member - the same pattern as
// gameVersion(), launchOptions(), levelStats(), clip(), level(), wad() and randomness().
GameSession& gameSession();
} // namespace Doom
