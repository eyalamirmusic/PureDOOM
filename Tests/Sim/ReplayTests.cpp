// The engine runs more than one scenario per process.
//
// The suite has always booted a fresh process per test, because doom_init cannot
// run twice - Z_Init would leak the 12 MB arena. That is still true. But it turns
// out the engine never needed re-initialising to run a second scenario: loading a
// level (Doom::initNewGame -> Doom::setupLevel) resets the whole simulation, and since Step 4
// gave the geometry to Doom::Level (which assigns fresh vectors each load) that
// reset is clean.
//
// This is the test that says so, and it is the one the plan called "boot twice":
// play a demo to the end, queue it again in the same process, play it again, and
// the two runs must agree tic for tic. If the level-load reset left anything of
// the first run behind - a stale sector, an un-freed thinker, a random index that
// did not clear - the second run diverges.
//
// It is also the only test that exercises Doom::Level's *reload* path; the
// geometry view test loads one level, this one loads a second over the first.

#include "../Common.h"
#include "../DemoReplay.h"

#include <algorithm>
#include <cstdio>

using namespace nano;
using namespace DoomTests;

namespace
{
Hashes playCurrentDemo()
{
    auto hashes = Hashes {};

    for (auto tic = 0; tic < 30000; ++tic)
    {
        if (!doomSimRunTic())
            break;

        if (doomSimInLevel())
            hashes.push_back(doomSimStateHash());
    }

    return hashes;
}

auto tReplayIsDeterministic = test("Sim/replaysASecondTimeInOneProcess") = []
{
    check(doomSimBoot("demo1") != 0, "engine booted");

    auto first = playCurrentDemo();
    check(!first.empty(), "the first run drove tics");

    doomSimReplayDemo("demo1");
    auto second = playCurrentDemo();
    check(!second.empty(), "the second run drove tics");

    const auto shared = std::min(first.size(), second.size());

    for (auto i = std::size_t {0}; i < shared; ++i)
    {
        if (first[i] == second[i])
            continue;

        std::printf("\nreplays diverged at tic %zu: the level-load reset left "
                    "something of the first run behind.\n\n",
                    i);
        check(false, "the second run matches the first tic for tic");
        return;
    }

    check(first.size() == second.size(), "both runs are the same length");
};

// The stronger version: a *different* level loaded over the first. demo1 and
// demo2 are different maps with different vertex, seg and sector counts, so
// playing demo2 after demo1 drives Doom::Level::assign to a different size than
// the level already sitting in its vectors - the grow/shrink path a single demo
// never touches. The proof is that demo2, loaded second, still produces exactly
// what demo2 produces from a fresh boot: its own committed golden.
auto tSecondLevelLoadsClean = test("Sim/aDifferentLevelLoadsOverThePrevious") = []
{
    check(doomSimBoot("demo1") != 0, "engine booted");
    playCurrentDemo();

    doomSimReplayDemo("demo2");
    auto second = playCurrentDemo();
    check(!second.empty(), "demo2 drove tics after demo1");

    auto golden = readGolden("demo2", "hashes");
    check(!golden.empty(), "demo2 golden exists");

    const auto shared = std::min(second.size(), golden.size());

    for (auto i = std::size_t {0}; i < shared; ++i)
    {
        if (second[i] == golden[i])
            continue;

        std::printf(
            "\ndemo2 loaded over demo1 diverged from its own golden at "
            "tic %zu: the reload of a different-sized level is not clean.\n\n",
            i);
        check(false, "demo2 after demo1 matches demo2's fresh-boot golden");
        return;
    }

    check(second.size() == golden.size(),
          "demo2 after demo1 is the same length as its golden");
};
} // namespace
