// The Level object owns the level's static geometry (Sim/Level.h), and the loaders
// wire it together from raw WAD lump numbers.
//
// The demos already exercise the geometry exhaustively: every rendered frame and
// every Doom::tryMove reads it, and those are bit-identical, so the data is right.
// What the demos cannot see is a cross-reference that points *outside* the vector
// it names. Nothing in the loaders bounds-checks a lump number, so a bad one is a
// pointer into whatever follows the vector - which a demo would notice, if at all,
// as a segfault or an unexplained desync a long way from the cause.
//
// This used to check a different invariant: that the vanilla pointer-and-count
// globals (vertexes, numsegs, sectors, ...) still viewed the Level's vectors after
// a load, since a loader that resized a vector and forgot to refresh its global
// would leave the global dangling. Those globals are gone - readers index the
// vectors directly - so that failure mode no longer exists to be tested for.

#include "../Common.h"

using namespace nano;

namespace
{
auto tGeometryWellFormed = test("Sim/levelGeometryIsWellFormed") = []
{
    check(doomSimBoot("demo1") != 0, "engine booted");

    // Run until the deferred demo is driving an actual level.
    auto tics = 0;
    while (!doomSimInLevel() && doomSimRunTic() && tics < 500)
        ++tics;

    check(doomSimInLevel(), "the demo reached a level");
    check(doomSimLevelGeometryIsWellFormed() != 0,
          "every cross-reference the loaders built lands inside its own vector");
};
} // namespace
