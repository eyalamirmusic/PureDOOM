// What no golden can see: whether an owner gave the memory back.
//
// The demo and frame goldens hash the world and the picture, so a leak is invisible
// to them by construction - leaked memory changes nothing about the simulation until
// the process runs out. That makes this the only kind of check that reaches an owner
// whose entire job is to release something, which is what Step 9 strand (b) is about
// (REFACTOR.md).
//
// These live in SimTests rather than alongside the other Engine cases in
// PrimitiveTests, and the reason is worth knowing before adding another: booting the
// engine needs the IWAD, which Tests/TestMain.cpp finds by setting DOOMWADDIR from
// PUREDOOM_ROOT_DIR - and only SimTests links that main. PrimitiveTests takes
// NanoTest's default one, so a booting test placed there passes when the binary is
// run from the repository root by hand and fails under ctest, which runs it from
// somewhere else. That is exactly how this file came to exist.

#include "../Common.h"
#include "../SimProbe.h"

#include <DOOM/Engine/Engine.h>

using namespace nano;
using namespace Doom;

namespace
{
// The level pool (Sim/LevelPool) is the last hand-rolled owner in the engine, and it
// cannot become a container: its blocks are variable-sized, hold polymorphic Thinkers
// whose addresses Sim/SaveGame serialises and the thinker list threads, and so can
// never be moved. RAII there means owning the *release*, which is a destructor - and
// this is what holds it.
//
// It is sharp because Doom::host().malloc is now almost exclusively the pool's own
// allocator:
// the RAII sweep moved nearly everything else onto Doom::Vector, which allocates through
// operator new and is not counted here. What the counter still sees besides the pool
// is host-side and deliberately outlives the Engine - Host/System's buffer,
// Host/Sound's audio-blocked paddedsfx, DoomMain's response-file argv - so the
// post-reset figure lands back exactly on the post-boot one.
//
// Measured against the code this replaced: 107 blocks after boot, +120 for E1M1, and
// all 227 still outstanding after resetEngine(). The pool freed nothing, because a
// bare `{ LevelChunk* head; }` has no destructor and only Doom::loadLevel's explicit
// freeLevelAllocations ever emptied it - and teardown never called that. The leak
// became reachable the moment strand (a) made the Engine constructible.
auto tResetEngineReleasesTheLevelPool =
    test("Engine/resetEngineReleasesTheLevelPool") = []
{
    // Before the boot, so every block the engine takes is one this counter saw.
    doomSimCountAllocations();

    check(doomSimBoot() != 0, "the engine booted");
    const int afterBoot = doomSimLiveAllocations();

    check(doomSimLoadLevel(1, 1, 2) != 0, "E1M1 loaded");
    const int withLevel = doomSimLiveAllocations();

    // Guards the test itself: if loading a level ever stopped allocating through the
    // pool, the assertion below would pass while measuring nothing at all.
    check(withLevel > afterBoot,
          "loading a level took blocks from the pool for its mobjs and specials");

    resetEngine();

    check(doomSimLiveAllocations() <= afterBoot,
          "destroying the Engine returned every block the level pool took");
};
} // namespace
