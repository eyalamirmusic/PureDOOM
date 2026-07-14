#include "../DemoReplay.h"

#include <algorithm>

using namespace nano;
using namespace DoomTests;

// The shareware WAD's three attract-mode demos. Between them they run the
// player through E1M1 and E1M3 under recorded input: shooting, taking damage,
// dying, respawning, opening doors, riding lifts, and letting a level's worth
// of monsters think. Every one of those paths goes through the code a physics
// or game-logic refactor touches.
//
// The demo is the assertion. There is nothing to hand-write: identical input
// against a deterministic simulation must produce an identical world, so any
// change in behaviour shows up as a desync, at the tic it first happened.
auto tDemo1 = test("Sim/demo1") = [] { checkDemoMatchesGolden("demo1"); };
auto tDemo2 = test("Sim/demo2") = [] { checkDemoMatchesGolden("demo2"); };
auto tDemo3 = test("Sim/demo3") = [] { checkDemoMatchesGolden("demo3"); };

// A golden test that passes vacuously is worse than no test: if the engine came
// up broken and simulated nothing, every run would agree on nothing and the
// suite would stay green through any refactor at all. So the demo is checked to
// have actually happened - a level's worth of monsters, a player who moves and
// bleeds - before its hashes are trusted to mean anything.
auto tDemoIsSubstantial = test("Sim/demoActuallySimulates") = []
{
    check(doomSimBoot("demo1") != 0);

    auto tics = 0;
    auto startX = 0;
    auto startY = 0;
    auto moved = false;
    auto tookDamage = false;
    auto peakMobjs = 0;

    while (doomSimRunTic() && tics < 30000)
    {
        if (!doomSimInLevel())
            continue;

        if (tics == 0)
        {
            startX = doomSimPlayerX();
            startY = doomSimPlayerY();
        }

        if (doomSimPlayerX() != startX || doomSimPlayerY() != startY)
            moved = true;

        if (doomSimPlayerHealth() < 100)
            tookDamage = true;

        peakMobjs = std::max(peakMobjs, doomSimMobjCount());
        ++tics;
    }

    check(tics > 1000, "the demo simulated a real stretch of play");
    check(moved, "the player moved");
    check(tookDamage, "the player took damage, so combat ran");
    check(peakMobjs > 100, "a level's worth of objects were thinking");
};
