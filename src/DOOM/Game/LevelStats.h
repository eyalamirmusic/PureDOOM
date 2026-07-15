#pragma once

namespace Doom
{
// The current level's progress. P_SetupLevel counts the map's monsters, items and secret
// sectors into the totals; the playsim bumps them as the player kills, picks up and finds
// (each is what the intermission tallies against the map's totals). levelstarttic records
// the gametic the level began at, and leveltime the tics elapsed in it - the level clock
// the specials time doors and lifts against, and what the intermission shows as the par
// comparison. All of it resets at level start.
//
// The first cluster of doomstat.h's game state to move off the loose globals into the
// Engine (REFACTOR.md, Step 5) - the renderer's own state having gone first (ViewPoint
// ... RenderScratch). All five were externed only in doomstat.h with no local extern; the
// storage moves off the Game/Game.cpp (totals + levelstarttic) and p_tick.cpp (leveltime)
// file-scope globals and the vanilla names are references onto these members. The values
// are hashed by the simulation probe, but a reference reads the identical value, so the
// move is golden-neutral like the rest.
struct LevelStats
{
    // Tallies shown at intermission, against the map's own totals.
    int totalkills = 0;
    int totalitems = 0;
    int totalsecret = 0;

    // The level clock: the gametic it started at, and the tics elapsed in it.
    int levelstarttic = 0;
    int leveltime = 0;
};

// The one LevelStats, a view onto the Engine's member - the same pattern as
// renderScratch(), graphicsData(), viewPoint(), clip(), level(), wad() and randomness().
LevelStats& levelStats();
} // namespace Doom
