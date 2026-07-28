// What a frame of world geometry costs the CPU, measured rather than reasoned
// about.
//
// Three items in EACP_PLAN.md - P3 (instanced billboards), P4 (per-sector
// heights, so walls and flats stop being rebuilt) and E6 (texture arrays, so the
// per-texture draws collapse) - all attack the cost of Engine::buildGeometry and
// the upload that follows it, from three different sides. They should not all be
// built, and nothing in this repository had ever measured the thing they aim at.
// This is that measurement.
//
// It is a benchmark, not a test: it asserts nothing and no ctest case runs it.
// The output is a number to read, and re-reading it after a change is how the
// change gets judged. Build and run it in Release - a Debug figure here is a
// measurement of the standard library's bounds checks.
//
// It needs no GPU, for the same reason Tests/Port does: EngineAccess.cpp
// includes only the engine's headers and hands out plain data, so the builder
// runs headlessly and the only thing missing from the real frame is the upload
// itself (View::renderWorld's worldBuffer.update), which is measured in the app.
//
// The camera is the engine's own rather than View::viewCamera's interpolated
// one. That changes nothing it measures: the emitter culls nothing, so where the
// camera is decides only which way the billboards face and where the sky
// cylinder is centred, never how much work is done.

#include "SimProbe.h"

#include <EngineAccess.h>

#include <DOOM/Game/GameSession.h>
#include <DOOM/Render/GraphicsData.h>
#include <DOOM/Sim/Level.h>
#include <DOOM/Sim/ThinkerList.h>

#include <eacp/Core/Utils/Environment.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;

// The app's own ceilings (PureDoom::maxVertices / maxDraws), so this asks for a
// frame the same shape the renderer does.
constexpr auto maxVertices = 262144;
constexpr auto maxDraws = 2048;

// The display refreshes two to four times per tic, and the world is rebuilt on
// every one of them - the billboards and the sky are built around the camera
// being drawn from, which moves between tics. Three is the middle of that range
// and the ratio a 100Hz display gives against DOOM's 35Hz.
constexpr auto framesPerTic = 3;

// A demo runs ~2,000 tics and the first few are the title screen, where there is
// no world to build.
constexpr auto ticCeiling = 30000;

// One vertex span asked for with room for nothing: every texture's run fails the
// layout's bounds check, so its cursor comes back -1 and the second pass walks
// the whole world writing not one vertex. The difference against a real frame is
// what the stores cost; half of what remains is one walk.
constexpr auto walkOnlyVertices = 1;

struct Distribution
{
    double mean = 0.0;
    double median = 0.0;
    double p95 = 0.0;
    double max = 0.0;
};

Distribution distributionOf(std::vector<double> samples)
{
    if (samples.empty())
        return {};

    std::sort(samples.begin(), samples.end());

    auto total = 0.0;

    for (auto sample: samples)
        total += sample;

    auto at = [&](double fraction)
    {
        auto index = static_cast<std::size_t>(fraction * (double) samples.size());

        return samples[std::min(index, samples.size() - 1)];
    };

    return {total / (double) samples.size(), at(0.5), at(0.95), samples.back()};
}

// What one demo's worth of frames came to.
struct Run
{
    std::string demo;
    int episode = 0;
    int map = 0;

    int lines = 0;
    int subsectors = 0;
    int mobjs = 0;

    int frames = 0;

    Distribution build;
    Distribution walk;

    double vertices = 0.0;
    double draws = 0.0;
    double fuzzDraws = 0.0;
    double bytes = 0.0;

    // The same vertices split by what kind of surface they are, which is what
    // decides how much of the frame P4 could make static: walls and flats move
    // only when a door or a lift does, while a billboard is rebuilt around the
    // camera every refresh by construction.
    double wallVertices = 0.0;
    double flatVertices = 0.0;
    double spriteVertices = 0.0;

    std::uint64_t hash = 0xcbf29ce484222325ull;
};

// The id space EngineAccess lays out: the wall textures, then the flats, then
// the sprite lumps. The sky is drawn with a wall texture and counts as one.
int flatBase()
{
    return Doom::graphicsData().numtextures;
}

int spriteBase()
{
    return flatBase() + Doom::graphicsData().numflats;
}

// Every vertex and every draw the demo emitted, mixed bit for bit.
//
// A benchmark that only reports a time cannot tell a change that made the
// builder faster from one that made it emit something else, and the frame
// goldens cannot help - they run the software renderer, which never executes a
// line of this. So the run carries its own answer: an optimisation is honest
// exactly when the number below is unchanged.
//
// Hashed off the float bits rather than their values, which is what makes it
// exact. Every 16th frame, because hashing 5 GB costs more than the measurement
// does and a demo diverges for good once it diverges at all.
constexpr auto framesPerHash = 16;

std::uint64_t mix(std::uint64_t hash, const void* bytes, std::size_t count)
{
    const auto* at = static_cast<const unsigned char*>(bytes);

    for (auto i = std::size_t {0}; i < count; ++i)
    {
        hash ^= at[i];
        hash *= 0x100000001b3ull;
    }

    return hash;
}

int liveMobjs()
{
    auto count = 0;

    for (auto& thinker: Doom::thinkerList())
        if (thinker->asMobj() != nullptr && !thinker->removed)
            count++;

    return count;
}

double microsSince(Clock::time_point start)
{
    return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
}

struct Buffers
{
    Buffers()
    {
        vertices.resize(maxVertices);
        draws.resize(maxDraws);
    }

    std::vector<PureDoom::Engine::WorldVertex> vertices;
    std::vector<PureDoom::Engine::TextureDraw> draws;
};

Run measure(const std::string& demo, Buffers& buffers)
{
    auto run = Run {};
    run.demo = demo;

    auto builds = std::vector<double> {};
    auto walks = std::vector<double> {};

    for (auto tic = 0; tic < ticCeiling; ++tic)
    {
        PureDoom::Engine::snapshotTic();

        if (!doomSimRunTic())
            break;

        if (!doomSimInLevel())
            continue;

        auto camera = PureDoom::Engine::camera();

        for (auto frame = 0; frame < framesPerTic; ++frame)
        {
            auto alpha = (float) frame / (float) framesPerTic;

            // The same frame with nowhere to put itself: every run fails the
            // layout's bounds check, so the second pass walks the whole world
            // and stores none of it.
            auto walkOnly = [&]
            {
                auto started = Clock::now();
                PureDoom::Engine::buildGeometry(
                    camera,
                    alpha,
                    {std::span {buffers.vertices}.first(walkOnlyVertices),
                     buffers.draws});

                return microsSince(started);
            };

            auto full = [&]
            {
                auto started = Clock::now();
                auto world = PureDoom::Engine::buildGeometry(
                    camera, alpha, {buffers.vertices, buffers.draws});

                return std::pair {microsSince(started), world};
            };

            // Whichever runs second finds the level's geometry in cache, so
            // running one of them first every time would charge the difference
            // to the other. They alternate instead.
            auto walkFirst = (run.frames % 2) == 0;
            auto walked = walkFirst ? walkOnly() : 0.0;
            auto [elapsed, world] = full();

            if (!walkFirst)
                walked = walkOnly();

            if (world.draws.empty())
                continue;

            builds.push_back(elapsed);
            walks.push_back(walked);

            run.frames++;
            run.vertices += (double) world.vertices.size();
            run.draws += (double) world.draws.size();
            run.fuzzDraws += (double) world.fuzzDraws.size();
            run.bytes += (double) world.vertices.size_bytes();

            if (run.frames % framesPerHash == 0)
            {
                run.hash = mix(
                    run.hash, world.vertices.data(), world.vertices.size_bytes());
                run.hash =
                    mix(run.hash, world.draws.data(), world.draws.size_bytes());
                run.hash = mix(
                    run.hash, world.fuzzDraws.data(), world.fuzzDraws.size_bytes());
            }

            for (const auto& draw: world.draws)
            {
                auto count = (double) draw.vertexCount;

                if (draw.textureId >= spriteBase())
                    run.spriteVertices += count;
                else if (draw.textureId >= flatBase())
                    run.flatVertices += count;
                else
                    run.wallVertices += count;
            }
        }
    }

    run.episode = Doom::gameSession().gameepisode;
    run.map = Doom::gameSession().gamemap;
    run.lines = (int) Doom::level().lines.size();
    run.subsectors = (int) Doom::level().subsectors.size();
    run.mobjs = liveMobjs();

    run.build = distributionOf(builds);
    run.walk = distributionOf(walks);

    if (run.frames > 0)
    {
        auto frames = (double) run.frames;

        run.vertices /= frames;
        run.draws /= frames;
        run.fuzzDraws /= frames;
        run.bytes /= frames;
        run.wallVertices /= frames;
        run.flatVertices /= frames;
        run.spriteVertices /= frames;
    }

    return run;
}

void report(const Run& run)
{
    std::printf("\n%s  E%dM%d  %d lines  %d subsectors  %d things\n",
                run.demo.c_str(),
                run.episode,
                run.map,
                run.lines,
                run.subsectors,
                run.mobjs);

    std::printf("  %d frames measured\n", run.frames);

    std::printf("  buildGeometry   mean %7.1fus  median %7.1fus  p95 %7.1fus  "
                "max %7.1fus\n",
                run.build.mean,
                run.build.median,
                run.build.p95,
                run.build.max);

    std::printf("  both walks only mean %7.1fus  median %7.1fus   "
                "(stores %5.1fus, one walk %5.1fus)\n",
                run.walk.mean,
                run.walk.median,
                run.build.mean - run.walk.mean,
                run.walk.mean / 2.0);

    std::printf("  per frame       %8.0f vertices  %6.1f KB  %5.1f draws"
                "  %4.1f fuzz draws\n",
                run.vertices,
                run.bytes / 1024.0,
                run.draws,
                run.fuzzDraws);

    auto share = [&](double part)
    { return run.vertices > 0.0 ? 100.0 * part / run.vertices : 0.0; };

    std::printf("  of which        walls %5.1f%%  flats %5.1f%%  sprites %5.1f%%"
                "  (%.0f / %.0f / %.0f)\n",
                share(run.wallVertices),
                share(run.flatVertices),
                share(run.spriteVertices),
                run.wallVertices,
                run.flatVertices,
                run.spriteVertices);

    // What the same cost looks like against the two clocks that matter: the
    // tic the engine runs on, and one refresh of a 120Hz display.
    std::printf("  cost per frame  %5.2f%% of a 8.33ms refresh  %5.2f%% of a "
                "28.6ms tic\n",
                100.0 * run.build.mean / 8333.0,
                100.0 * run.build.mean / 28571.0);

    std::printf("  geometry hash   %016llx\n", (unsigned long long) run.hash);
}
} // namespace

int main()
{
    // The engine locates WADs through DOOMWADDIR, falling back to the working
    // directory - which says nothing about where this was run from. Not
    // overwritten, so a developer pointing at another WAD still gets it.
    if (!eacp::getEnv("DOOMWADDIR"))
        eacp::setEnv("DOOMWADDIR", PUREDOOM_ROOT_DIR);

    if (!doomSimBoot("demo1"))
    {
        std::printf("the engine did not boot\n");
        return 1;
    }

    auto buffers = Buffers {};

    std::printf("Engine::buildGeometry, %d frames per tic\n", framesPerTic);

    report(measure("demo1", buffers));

    for (const auto* demo: {"demo2", "demo3"})
    {
        doomSimReplayDemo(demo);
        report(measure(demo, buffers));
    }

    std::printf("\n");

    return 0;
}
