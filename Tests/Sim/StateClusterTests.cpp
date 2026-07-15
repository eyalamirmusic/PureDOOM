// The state clusters the file-scope-statics sweep moved into the Engine (REFACTOR.md, Step 5).
//
// Most are pinned by the demo/frame goldens already - the status bar and HUD are drawn every tic,
// and a wrong migration moves a pixel. But a few carry *data* the goldens do not see: the par-time
// and movement-speed tables are golden-neutral (the demos override the built ticcmd and never reach
// a par-time readout) yet load-bearing - read straight out, never reset - so a mistyped default
// would pass every golden. These construct a fresh cluster and pin the values that reproduce
// vanilla's initializers, and check the free accessors still view the one Engine's members (the
// same guarantee EngineTests makes for random/wad/level). They boot nothing - the clusters are
// trivially constructible.

#include "../Common.h"

#include <DOOM/Engine/Engine.h>

using namespace nano;
using namespace Doom;

namespace
{
// The DOOM / DOOM II par times, read straight into wminfo.partime on level completion. Golden-
// neutral (a demo rarely reaches the intermission) but exact, so pin the corners and a few interior
// values against the 1993 tables.
auto tParTimes = test("StateClusters/parTimesMatchVanilla") = []
{
    auto p = ParTimes {};

    check(p.pars[1][1] == 30, "E1M1 par is 30");
    check(p.pars[1][9] == 165, "E1M9 par is 165");
    check(p.pars[2][6] == 360, "E2M6 par is 360");
    check(p.pars[3][9] == 135, "E3M9 par is 135");

    check(p.cpars[0] == 30, "MAP01 par is 30");
    check(p.cpars[16] == 420, "MAP17 par is 420");
    check(p.cpars[31] == 30, "MAP32 par is 30");
};

// The ticcmd movement-speed tables G_BuildTiccmd indexes. Golden-neutral (demo playback overrides
// the command it builds) but load-bearing - a wrong walk/run speed would change every live game.
auto tMovementSpeeds = test("StateClusters/movementSpeedsMatchVanilla") = []
{
    auto m = MovementSpeeds {};

    check(m.forwardmove[0] == 0x19 && m.forwardmove[1] == 0x32,
          "forwardmove walk/run");
    check(m.sidemove[0] == 0x18 && m.sidemove[1] == 0x28, "sidemove walk/run");
    check(m.angleturn[0] == 640 && m.angleturn[1] == 1280 && m.angleturn[2] == 320,
          "angleturn fast/faster/slow");
};

// The remaining clusters' non-trivial defaults - cheap fidelity checks that a fresh cluster starts
// where vanilla's zero/-1/constant initializers put it.
auto tOtherDefaults = test("StateClusters/otherClusterDefaults") = []
{
    check(TimeDemo {}.timingdemo == false && TimeDemo {}.starttime == 0,
          "TimeDemo clear");
    check(PendingCommands {}.sendpause == false
              && PendingCommands {}.sendsave == false,
          "PendingCommands clear");
    check(HudChat {}.queueSize == 128 && HudChat {}.head == 0
              && HudChat {}.tail == 0,
          "HudChat ring empty");
    check(HudMessage {}.message_on == false && HudMessage {}.message_counter == 0,
          "HudMessage clear");
    check(StatusBarFace {}.st_oldhealth == -1,
          "StatusBarFace st_oldhealth sentinel");
    check(StatusBarGraphics {}.numFaces == 42
              && StatusBarGraphics {}.sbar == nullptr,
          "StatusBarGraphics face count and null patches");
    check(StatusBarState {}.veryfirsttime == 1
              && StatusBarState {}.st_stopped == true,
          "StatusBarState one-time gate and parked flag");
    check(AutomapView {}.leveljuststarted == 1 && AutomapView {}.stopped == true
              && AutomapView {}.finit_width == SCREENWIDTH
              && AutomapView {}.finit_height == SCREENHEIGHT - 32,
          "AutomapView kluge flag, closed flag and frame size");
};

// The free accessors are views onto the one Engine's members, the same property EngineTests pins
// for random/wad/level - a future change that made one a separate singleton would split the world.
auto tAccessorsViewTheEngine = test("StateClusters/accessorsViewTheOneEngine") = []
{
    check(&parTimes() == &engine().parTimes, "parTimes()");
    check(&movementSpeeds() == &engine().movementSpeeds, "movementSpeeds()");
    check(&timeDemo() == &engine().timeDemo, "timeDemo()");
    check(&pendingCommands() == &engine().pendingCommands, "pendingCommands()");
    check(&hudMessage() == &engine().hudMessage, "hudMessage()");
    check(&hudChat() == &engine().hudChat, "hudChat()");
    check(&hudState() == &engine().hudState, "hudState()");
    check(&statusBarFace() == &engine().statusBarFace, "statusBarFace()");
    check(&statusBarWidgets() == &engine().statusBarWidgets, "statusBarWidgets()");
    check(&statusBarGraphics() == &engine().statusBarGraphics,
          "statusBarGraphics()");
    check(&statusBarState() == &engine().statusBarState, "statusBarState()");
    check(&automapView() == &engine().automapView, "automapView()");
};
} // namespace
