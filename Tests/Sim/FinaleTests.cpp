#include "../FinaleReplay.h"

using namespace nano;
using namespace DoomTests;

// UI/Finale.cpp has no demo coverage - no attract-mode demo reaches an episode's
// exit - so it gets its own frame golden before its 11 reference aliases are
// retired, driven by Doom::startFinale() directly the way the automap golden
// drives synthetic key events. checkFinaleMatchesGolden loads E1M8, starts the
// finale and lets it run - the text crawl, the transition, and the HELP2 screen
// that follows - hashing every frame against Tests/Goldens/finale.frames; see
// FinaleReplay.h for what it drives and what it deliberately cannot reach.
//
// Run twice (./SimTests --test Sim/finale, twice, diffing the output) while
// building this golden, to confirm the M_Random the screen melt consumes (see
// FinaleReplay.h) does not make the frames non-reproducible. Both runs matched.
auto tFinale = test("Sim/finale") = [] { checkFinaleMatchesGolden(); };
