// The Level object owns the level's static geometry (Sim/Level.h), and the
// vanilla globals - vertexes, numsegs, sectors, ... - are views onto it.
//
// The demos already exercise the geometry exhaustively: every rendered frame and
// every P_TryMove reads it, and those are bit-identical, so the data is right.
// What the demos cannot see is the *view invariant* - that the globals still
// point at the Level's vectors after a load. A future loader that resizes a
// vector and forgets to refresh its global would reallocate the storage and leave
// the global dangling; the demos might survive it by luck of the allocator, and
// this would not.

#include "../Common.h"

using namespace nano;

namespace
{
auto tGeometryViews = test("Sim/levelGeometryIsViewedFromLevelObject") = []
{
    check(doomSimBoot("demo1") != 0, "engine booted");

    // Run until the deferred demo is driving an actual level.
    auto tics = 0;
    while (!doomSimInLevel() && doomSimRunTic() && tics < 500)
        ++tics;

    check(doomSimInLevel(), "the demo reached a level");
    check(doomSimGeometryViewsConsistent() != 0,
          "the geometry globals are consistent views onto Doom::Level");
};
} // namespace
