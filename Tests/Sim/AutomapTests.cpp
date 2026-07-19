#include "../AutomapReplay.h"

using namespace nano;
using namespace DoomTests;

// UI/Automap.cpp has no demo coverage - nothing in a recorded .lmp opens the
// automap - so it gets its own frame golden before its 30 reference aliases are
// retired, driven by synthetic key events the way m_menu's golden was.
// checkAutomapMatchesGolden loads E1M1, opens the map and walks it (follow
// on/off, hand-panning, zoom, the big overview, grid, marks) and holds every
// frame against Tests/Goldens/automap.frames; see AutomapReplay.h for the script
// and what it deliberately avoids.
auto tAutomap = test("Sim/automap") = [] { checkAutomapMatchesGolden(); };
