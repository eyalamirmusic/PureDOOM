#pragma once

#include "Common.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace DoomTests
{
// The simulation's state after each tic of a demo.
using TicHashes = std::vector<std::uint64_t>;

// Plays a demo through to its end, hashing the world after every tic.
inline TicHashes replayDemo(const std::string& demo)
{
    auto hashes = TicHashes {};

    nano::check(doomSimBoot(demo.c_str()) != 0, "engine booted");

    // Demos are finite, but a refactor that breaks the demo's own bookkeeping
    // could leave one running forever; the ceiling keeps a broken build from
    // hanging the suite instead of failing it.
    constexpr auto ticCeiling = 30000;

    for (auto tic = 0; tic < ticCeiling; ++tic)
    {
        if (!doomSimRunTic())
            break;

        // The engine spends its first few tics on the title screen, before the
        // deferred demo takes over.
        if (doomSimInLevel())
            hashes.push_back(doomSimStateHash());
    }

    return hashes;
}

inline std::string goldenPath(const std::string& demo)
{
    return std::string {PUREDOOM_TESTS_DIR} + "/Goldens/" + demo + ".hashes";
}

// Set DOOM_UPDATE_GOLDENS=1 to re-record. That is the deliberate act of saying
// "this behaviour change was intended" - the goldens pin the simulation as it
// stands, bugs included, which is exactly what a behaviour-preserving refactor
// wants held still.
inline bool updatingGoldens()
{
    const auto* flag = std::getenv("DOOM_UPDATE_GOLDENS");
    return flag && flag[0] == '1';
}

inline void writeGolden(const std::string& demo, const TicHashes& hashes)
{
    auto file = std::ofstream {goldenPath(demo)};

    file << "# " << demo << ": the DOOM simulation hashed after every tic.\n"
         << "# Re-record with DOOM_UPDATE_GOLDENS=1, and only on purpose.\n";

    for (auto hash: hashes)
        file << std::hex << hash << '\n';

    std::printf(
        "recorded %zu tics -> %s\n", hashes.size(), goldenPath(demo).c_str());
}

inline TicHashes readGolden(const std::string& demo)
{
    auto file = std::ifstream {goldenPath(demo)};
    auto hashes = TicHashes {};
    auto line = std::string {};

    while (std::getline(file, line))
    {
        if (line.empty() || line.front() == '#')
            continue;

        hashes.push_back(std::stoull(line, nullptr, 16));
    }

    return hashes;
}

// What the world looked like when it went wrong. A hash says only that two runs
// differ; this says where the player was standing and what the random index had
// reached, which is usually enough to name the culprit.
inline void reportDivergence(const std::string& demo, int tic, int goldenTics)
{
    std::printf(
        "\n%s desynced at tic %d (of %d recorded)\n", demo.c_str(), tic, goldenTics);

    std::printf("  the simulation now reads: rndindex=%d leveltime=%d\n"
                "                            player=(%d,%d) facing %d deg, "
                "health=%d\n"
                "                            live objects=%d\n",
                doomSimRndIndex(),
                doomSimLevelTime(),
                doomSimPlayerX(),
                doomSimPlayerY(),
                doomSimPlayerAngleDegrees(),
                doomSimPlayerHealth(),
                doomSimMobjCount());

    std::printf("  A demo is recorded input, so identical input must produce an\n"
                "  identical world. Something changed the simulation.\n"
                "  If that was deliberate, re-record: DOOM_UPDATE_GOLDENS=1\n\n");
}

// Replays a demo and holds it against its golden, failing at the FIRST tic that
// differs rather than merely reporting that the run as a whole did.
inline void checkDemoMatchesGolden(const std::string& demo)
{
    auto actual = replayDemo(demo);

    // Never record or compare an empty run. A golden of nothing would agree
    // with any future nothing, and the suite would stay green while testing
    // that the engine still fails to start.
    if (actual.empty())
    {
        std::printf("\n%s simulated no tics at all - the engine did not come "
                    "up.\n\n",
                    demo.c_str());
        nano::check(false, "the demo drove at least one tic");
        return;
    }

    if (updatingGoldens())
    {
        writeGolden(demo, actual);
        return;
    }

    auto golden = readGolden(demo);

    if (golden.empty())
    {
        std::printf("\nNo golden for %s. Record one with DOOM_UPDATE_GOLDENS=1\n\n",
                    demo.c_str());
        nano::check(false, "golden exists");
        return;
    }

    const auto shared = std::min(actual.size(), golden.size());

    for (auto tic = std::size_t {0}; tic < shared; ++tic)
    {
        if (actual[tic] == golden[tic])
            continue;

        reportDivergence(demo, (int) tic, (int) golden.size());
        nano::check(false, "simulation matches the golden");
        return;
    }

    // Same world every tic, but the demo ran for a different length: the sim
    // agreed and the demo's own bookkeeping did not.
    if (actual.size() != golden.size())
        std::printf("\n%s ran %zu tics, golden has %zu\n\n",
                    demo.c_str(),
                    actual.size(),
                    golden.size());

    nano::check(actual.size() == golden.size(), "the demo ran to the same length");
}
} // namespace DoomTests
