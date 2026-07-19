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

// The ticcmd movement-speed tables Doom::buildTiccmd indexes. Golden-neutral (demo playback overrides
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
    check(StatusBarFace {}.st_oldhealth == -1 && StatusBarFace {}.oldhealth == -1
              && StatusBarFace {}.lastattackdown == -1,
          "StatusBarFace -1 sentinels");
    check(StatusBarGraphics {}.numFaces == 42
              && StatusBarGraphics {}.sbar == nullptr,
          "StatusBarGraphics face count and null patches");
    check(StatusBarWidgets {}.largeammo == 1994,
          "StatusBarWidgets largeammo n/a sentinel");
    check(DisplayState {}.oldgamestate == (GameState) (-1)
              && DisplayState {}.borderdrawcount == 0,
          "DisplayState frame-diff defaults");
    check(StatusBarState {}.st_stopped == true, "StatusBarState parked flag");
    check(AutomapView {}.stopped == true && AutomapView {}.finit_width == SCREENWIDTH
              && AutomapView {}.finit_height == SCREENHEIGHT - 32,
          "AutomapView closed flag and frame size");
    // The one non-zero-looking default: IntermissionPhase zero-init lands on StatCount, since NoState is
    // -1, not 0. A migration that "helpfully" defaulted state to NoState would change the value.
    check(IntermissionState {}.state == StatCount
              && IntermissionState {}.snl_pointeron == false
              && IntermissionState {}.bg == nullptr,
          "IntermissionState zero-init state, cleared pointer flag, null patches");
    // Load-bearing true defaults. singletics drives the singletics loop/Doom::netUpdate quirk (a wrong
    // default would desync the demos); precache is golden-neutral (it changes only *when* graphics
    // load, not the pixels), so its default has no golden watching it and is pinned only here.
    check(EngineParams {}.precache == true && EngineParams {}.singletics == true
              && EngineParams {}.debugfile == nullptr,
          "EngineParams precache/singletics true, debugfile null");
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
    check(&intermissionState() == &engine().intermissionState,
          "intermissionState()");
    check(&finaleState() == &engine().finaleState, "finaleState()");
    check(&wipeState() == &engine().wipeState, "wipeState()");
    check(&menuState() == &engine().menuState, "menuState()");
    check(&compositeCache() == &engine().compositeCache, "compositeCache()");
    check(&wallScratch() == &engine().wallScratch, "wallScratch()");
    check(&spriteScratch() == &engine().spriteScratch, "spriteScratch()");
    check(&drawTables() == &engine().drawTables, "drawTables()");
    check(&solidSegs() == &engine().solidSegs, "solidSegs()");
    check(&planeScratch() == &engine().planeScratch, "planeScratch()");
    check(&renderMainState() == &engine().renderMainState, "renderMainState()");
    check(&actionScratch() == &engine().actionScratch, "actionScratch()");
    check(&weaponScratch() == &engine().weaponScratch, "weaponScratch()");
    check(&enemyAI() == &engine().enemyAI, "enemyAI()");
    check(&switchList() == &engine().switchList, "switchList()");
    check(&playerScratch() == &engine().playerScratch, "playerScratch()");
    check(&animatedSurfaces() == &engine().animatedSurfaces, "animatedSurfaces()");
    check(&levelPool() == &engine().levelPool, "levelPool()");
    check(&engineParams() == &engine().engineParams, "engineParams()");
    check(&soundState() == &engine().soundState, "soundState()");
};
} // namespace
