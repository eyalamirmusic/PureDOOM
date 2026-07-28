// The port's screen melt, tested headlessly - the third of the Tests/Port family,
// and there for the same reason as the other two: the port re-implements one of
// the engine's *decisions* somewhere no golden can see it.
//
// The engine composites its own melt into screens[0] and the frame goldens hash
// the result, so Sim/demo1 has watched a melt run since the day it was recorded.
// It has never watched the port's. What the port does instead is hand the two
// screens and the column offsets to a fragment shader and let it pick per pixel:
//
//     row r of column c = incoming[r]        where r <  slid[c / 2]
//                         outgoing[r - slid] where r >= slid[c / 2]
//
// That rule is doMelt's, written the other way round - the engine copies runs of
// rows into place as the columns move, and a shader has to answer for one pixel
// with no memory of the last frame. Getting it wrong draws a melt that still
// looks like a melt: a row out at the seam, or the two screens swapped in the
// half of the frame nobody is watching, is not something an eyeball catches while
// a level slides away in under a second.
//
// So this asserts the equality that makes the rule the engine's rather than a
// plausible imitation of it: recomposite from what Engine::buildWipe and
// Engine::wipeIncoming hand over, and hold it against screens[0], every tic of a
// real melt. It pins both transposes, the negative-offset clamp, and the seam.
//
// A level load wipes exactly as any level transition does (G_DoLoadLevel leaves
// wipegamestate mismatched against the new gamestate), so E1M1 is all this needs
// to reach one.

#include "../Common.h"

#include <EngineAccess.h>

#include <DOOM/DOOM.h>
#include <DOOM/Render/VideoState.h>

#include <cstdint>
#include <vector>

using namespace nano;
using namespace PureDoom;

namespace
{
constexpr auto e1 = 1;
constexpr auto m1 = 1;
constexpr auto skillMedium = 2;

// A melt runs about fifty tics; the guard is loose enough not to be the thing
// that ends the loop and tight enough to fail rather than hang.
constexpr auto meltGuard = 400;

struct Screens
{
    std::vector<std::uint8_t> outgoing =
        std::vector<std::uint8_t>(Engine::screenPixels);
    std::vector<std::uint8_t> incoming =
        std::vector<std::uint8_t>(Engine::screenPixels);
    std::vector<std::uint8_t> offsets =
        std::vector<std::uint8_t>(Engine::wipeColumns);
};

// What one tic of the melt says about itself, so the run as a whole can be
// checked for having actually melted rather than merely agreed.
struct Tally
{
    int mismatches = 0;
    int fromIncoming = 0;
    int fromOutgoing = 0;
    int differingPixels = 0;
};

Tally compositeAgrees(const Screens& screens)
{
    const auto* engine = Doom::videoState().screens[0];
    auto tally = Tally {};

    for (auto row = 0; row < Engine::screenHeight; ++row)
        for (auto x = 0; x < Engine::screenWidth; ++x)
        {
            const auto slid = (int) screens.offsets[x / 2];
            const auto fromIncoming = row < slid;
            const auto source = fromIncoming ? row : row - slid;

            const auto& from = fromIncoming ? screens.incoming : screens.outgoing;
            const auto expected = from[source * Engine::screenWidth + x];
            const auto actual = engine[row * Engine::screenWidth + x];

            if (fromIncoming)
                ++tally.fromIncoming;
            else
                ++tally.fromOutgoing;

            // The two screens are only worth comparing where they disagree: a
            // melt between two identical frames would composite correctly under
            // any rule at all, including a wrong one.
            if (screens.incoming[row * Engine::screenWidth + x]
                != screens.outgoing[row * Engine::screenWidth + x])
                ++tally.differingPixels;

            if (expected != actual)
                ++tally.mismatches;
        }

    return tally;
}

auto tMeltComposites = test("Port/meltCompositesLikeTheEngine") = []
{
    check(doomSimBoot() != 0, "engine booted headless, no demo queued");
    check(doomSimLoadLevel(e1, m1, skillMedium) != 0, "E1M1 loaded");

    check(doomSimStepTic() != 0, "the tic ran");
    check(doomSimIsWiping() != 0, "the level load arrived through a melt");

    auto screens = Screens {};
    auto tics = 0;
    auto slidTics = 0;
    auto worstMismatch = 0;
    auto worstTic = -1;
    auto leastDiffering = Engine::screenPixels;

    for (auto guard = 0; doomSimIsWiping() != 0 && guard < meltGuard; ++guard)
    {
        check(Engine::buildWipe(screens.outgoing, screens.offsets),
              "the outgoing frame came back while the melt was running");
        check(Engine::wipeIncoming(screens.incoming),
              "the incoming frame came back while the melt was running");

        const auto tally = compositeAgrees(screens);

        if (tally.mismatches > worstMismatch)
        {
            worstMismatch = tally.mismatches;
            worstTic = tics;
        }

        if (tally.differingPixels < leastDiffering)
            leastDiffering = tally.differingPixels;

        // A tic where both screens contribute is one where the seam is somewhere
        // inside the frame rather than off either end of it, which is the only
        // place the rule can be wrong without being obviously wrong.
        if (tally.fromIncoming > 0 && tally.fromOutgoing > 0)
            ++slidTics;

        ++tics;
        doomSimStepTic();
    }

    check(doomSimIsWiping() == 0, "the melt finished inside the guard");

    if (worstMismatch != 0)
        std::printf("\nmelt: %d of %d pixels disagree with the engine at tic %d\n"
                    "  The port composites the melt in a shader from the two\n"
                    "  screens and the column offsets; the engine copies rows of\n"
                    "  them into screens[0]. They are the same rule and must\n"
                    "  agree exactly.\n\n",
                    worstMismatch,
                    Engine::screenPixels,
                    worstTic);

    check(worstMismatch == 0, "every tic of the melt composites as the engine does");

    // The three that stop a green result from being vacuous: the melt ran, it
    // spent most of it part-way down, and the two screens it was mixing were
    // never the same picture.
    check(tics > 30, "the melt ran for the tics it takes");
    check(slidTics > 20, "most of it had the seam inside the frame");
    check(leastDiffering > Engine::screenPixels / 4,
          "the two screens differ over a quarter of the frame throughout");
};
} // namespace
