#include "../IntermissionReplay.h"

using namespace nano;
using namespace DoomTests;

// The between-levels intermission has no coverage of any kind - no demo
// completes a level, and unlike the menu/automap/finale it never got its own
// harness - so doCompleted, Doom::drawIntermission and doWorldDone have never
// run under this suite. checkLevelTransition drives the real thing end to end:
// Doom::exitLevel() out of E1M1, the whole scoreboard state machine, and the
// hand-over that loads E1M2. It asserts state transitions rather than frames;
// on its first sanitizer run it caught drawEL reading the cleared level-name
// table on the intermission's last tic (fixed at unloadIntermissionData).
auto tIntermission = test("Sim/intermission") = [] { checkLevelTransition(); };

// The picture the same transition draws, hashed every tic against
// Tests/Goldens/intermission.frames - the fourth screen no demo reaches, and the
// last to get a golden. Sharpness was measured before it was trusted, the way
// the automap's and the finale's were: moving SP_STATSX (UI/Intermission.cpp's
// left edge for the kills/items/secrets percentages) by one pixel fails this
// test alone, with the three demo goldens, the menu, the automap and the finale
// all green through it.
//
// Run twice while recording, and diff, for the reason FinaleReplay.h gives: the
// melts and initAnimatedBack draw on M_Random, so the script is only as
// reproducible as that index is. Both runs matched.
auto tIntermissionFrames =
    test("Sim/intermissionFrames") = [] { checkIntermissionMatchesGolden(); };

// The focused regression test for the defect the transition surfaced: the
// intermission's last tic draws after endIntermission has unloaded, so the
// level-name table must survive the unload (see IntermissionReplay.h for the
// whole story). Red in any build while the unload clears the table - no
// sanitizer required - and the locality is the point: if this fails alone, the
// unload order regressed; if Sim/intermission fails too, the transition itself
// broke.
auto tIntermissionLastDraw = test("Sim/intermissionLastDraw") = []
{ checkIntermissionDataOutlivesItsLastDraw(); };
