#include "../MenuReplay.h"

using namespace nano;
using namespace DoomTests;

// m_menu has no demo coverage - nothing in a recorded .lmp opens a menu - so it
// gets its own frame golden before it is rewritten, driven by synthetic key
// events the way Step 0's renderer net was. checkMenuMatchesGolden walks the
// menus over the title screen and holds every frame against Tests/Goldens/
// menu.frames; see MenuReplay.h for the script and what it deliberately avoids.
auto tMenu = test("Sim/menu") = [] { checkMenuMatchesGolden(); };
