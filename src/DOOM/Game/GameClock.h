#pragma once

namespace Doom
{
// The game clock: gametic counts tics of simulation since the process started, advancing once
// per G_Ticker call. It is the monotonic session clock the whole engine times against - the
// level clock leveltime is measured from it (levelstarttic), demos are paced by it, and the
// netcode indexes its command rings by it. doomstat.h's gametic in "Internal parameters,
// fixed". (Distinct from leveltime, which resets each level and lives in Doom::LevelStats,
// and from maketic, the netcode's built-tic cursor in Doom::NetState.)
//
// A cluster of doomstat.h's game state moved off the loose globals into the Engine
// (REFACTOR.md, Step 5). Externed only in doomstat.h and defined in Game/Game.cpp above its
// namespace (a state owner); the vanilla name becomes a reference onto this member. A
// reference reads the identical value, so the move is golden-neutral; the app reads gametic
// through doomstat.h unchanged.
struct GameClock
{
    int gametic = 0; // tics of simulation since process start
};

// The one GameClock, a view onto the Engine's member - the same pattern as
// mapSpawns(), netState(), overlayState(), refreshFlags(), demoState() and gameFlow().
GameClock& gameClock();
} // namespace Doom
