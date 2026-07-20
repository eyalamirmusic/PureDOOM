#pragma once

#include "Common.h"

#include <eacp/Core/Utils/Environment.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace DoomTests
{
using Hashes = std::vector<std::uint64_t>;

// What a demo leaves behind: the simulation after every tic, and the picture the
// software renderer drew of it. Both come out of one replay because the engine
// boots once per process and a second pass is not on offer.
struct DemoRun
{
    Hashes sim;
    Hashes frames;
};

// The frame is 64,000 bytes and the demos are 11,410 tics; hashing every one of
// them would cost more than the whole suite currently does, and buy very little.
// A quarter of them still catches any renderer change that lasts longer than a
// tenth of a second, and nothing in the renderer is briefer than that.
constexpr auto ticsPerFrameHash = 4;

// Plays a demo through to its end, hashing the world after every tic.
inline DemoRun replayDemo(const std::string& demo)
{
    auto run = DemoRun {};

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
        if (!doomSimInLevel())
            continue;

        if (run.sim.size() % ticsPerFrameHash == 0)
            run.frames.push_back(doomSimFrameHash());

        run.sim.push_back(doomSimStateHash());
    }

    return run;
}

// The simulation and the picture are held against separate goldens, so a failure
// says which of the two moved. A renderer refactor that desyncs the simulation is
// a very different bug from one that merely draws it wrong, and the suite should
// not make you guess which you have.
inline std::string goldenPath(const std::string& demo, const std::string& kind)
{
    return std::string {PUREDOOM_TESTS_DIR} + "/Goldens/" + demo + "." + kind;
}

// Set DOOM_UPDATE_GOLDENS=1 to re-record. That is the deliberate act of saying
// "this behaviour change was intended" - the goldens pin the engine as it
// stands, bugs included, which is exactly what a behaviour-preserving refactor
// wants held still. The C++ refactor in REFACTOR.md never re-records: if a
// golden moves under it, the refactor was wrong.
inline bool updatingGoldens()
{
    // getEnvValue rather than getEnv: unset and empty mean the same thing here.
    return eacp::getEnvValue("DOOM_UPDATE_GOLDENS") == "1";
}

inline void writeGolden(const std::string& demo,
                        const std::string& kind,
                        const std::string& what,
                        const Hashes& hashes)
{
    auto file = std::ofstream {goldenPath(demo, kind)};

    file << "# " << demo << ": " << what << "\n"
         << "# Re-record with DOOM_UPDATE_GOLDENS=1, and only on purpose.\n";

    for (auto hash: hashes)
        file << std::hex << hash << '\n';

    std::printf(
        "recorded %zu -> %s\n", hashes.size(), goldenPath(demo, kind).c_str());
}

inline Hashes readGolden(const std::string& demo, const std::string& kind)
{
    auto file = std::ifstream {goldenPath(demo, kind)};
    auto hashes = Hashes {};
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

// Holds a run against its golden, failing at the FIRST entry that differs rather
// than merely reporting that the run as a whole did.
inline void checkAgainstGolden(const std::string& demo,
                               const std::string& kind,
                               const Hashes& actual,
                               std::string_view subject)
{
    auto golden = readGolden(demo, kind);

    if (golden.empty())
    {
        std::printf(
            "\nNo %s golden for %s. Record one with DOOM_UPDATE_GOLDENS=1\n\n",
            kind.c_str(),
            demo.c_str());
        nano::check(false, "golden exists");
        return;
    }

    const auto shared = std::min(actual.size(), golden.size());

    for (auto i = std::size_t {0}; i < shared; ++i)
    {
        if (actual[i] == golden[i])
            continue;

        if (kind == "hashes")
            reportDivergence(demo, (int) i, (int) golden.size());
        else
            std::printf(
                "\n%s: the rendered frame changed at tic %d\n"
                "  The simulation is a separate golden - if it is still\n"
                "  green, the world is right and the picture of it is not.\n\n",
                demo.c_str(),
                (int) i * ticsPerFrameHash);

        nano::check(false, subject);
        return;
    }

    if (actual.size() != golden.size())
        std::printf("\n%s.%s: %zu entries, golden has %zu\n\n",
                    demo.c_str(),
                    kind.c_str(),
                    actual.size(),
                    golden.size());

    nano::check(actual.size() == golden.size(), "the run is the same length");
}

inline void checkDemoMatchesGolden(const std::string& demo)
{
    auto run = replayDemo(demo);

    // Never record or compare an empty run. A golden of nothing would agree
    // with any future nothing, and the suite would stay green while testing
    // that the engine still fails to start.
    if (run.sim.empty())
    {
        std::printf("\n%s simulated no tics at all - the engine did not come "
                    "up.\n\n",
                    demo.c_str());
        nano::check(false, "the demo drove at least one tic");
        return;
    }

    if (updatingGoldens())
    {
        writeGolden(
            demo, "hashes", "the DOOM simulation hashed after every tic.", run.sim);
        writeGolden(demo,
                    "frames",
                    "the software frame and palette, hashed every 4th tic.",
                    run.frames);
        return;
    }

    checkAgainstGolden(demo, "hashes", run.sim, "simulation matches the golden");
    checkAgainstGolden(demo, "frames", run.frames, "renderer matches the golden");
}
} // namespace DoomTests
