#pragma once

namespace Doom
{
// The par times: the target completion time, in seconds, for each level. pars is the DOOM
// (episode/map) table indexed pars[gameepisode][gamemap]; cpars is the flat DOOM II table
// indexed cpars[gamemap - 1]. G_DoCompleted reads one of them into wminfo.partime (times 35, to
// tics) for the intermission scoreboard to draw. g_game's own file-scope tables, read by no other
// file and never written - fixed reference data, but off the loose globals all the same so the
// Engine owns every table rather than the process.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); the vanilla names
// pars/cpars become references-to-array onto these members, so every indexed read is unchanged.
// Read-only and reached only when a level completes, so the move is golden-neutral.
struct ParTimes
{
    // DOOM par times: row 0 is a filler (episodes are 1-based); the rest are E1-E3, maps 1-9.
    int pars[4][10] = {{0},
                       {0, 30, 75, 120, 90, 165, 180, 180, 30, 165},
                       {0, 90, 90, 90, 120, 90, 360, 240, 30, 170},
                       {0, 90, 45, 90, 150, 90, 90, 165, 30, 135}};

    // DOOM II par times, maps 1-32.
    int cpars[32] = {
        30,  90,  120, 120, 90,  150, 120, 120, 270, 90, //  1-10
        210, 150, 150, 150, 210, 150, 420, 150, 210, 150, // 11-20
        240, 150, 180, 150, 150, 300, 330, 420, 300, 180, // 21-30
        120, 30 // 31-32
    };
};

// The one ParTimes, a view onto the Engine's member - the same pattern as the other Game/
// clusters (intermissionInfo(), gameClock(), ...).
ParTimes& parTimes();
} // namespace Doom
