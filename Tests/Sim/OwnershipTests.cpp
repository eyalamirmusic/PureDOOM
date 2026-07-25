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
// Every mobj and thinker special the engine owns, released when the Engine goes.
//
// This used to hold a hand-rolled owner, Sim/LevelPool - an intrusive list of
// variable-sized blocks whose destructor was the whole point, added when strand (b)
// found that nothing freed it on teardown. That owner is gone: the ThinkerList is an
// OwnedVector<Thinker>, so the container that holds the *order* holds the *ownership*
// too, and there is no longer a way to allocate a thinker without registering it or
// to drop it from one list while the other still has it.
//
// The test survives the change unaltered, and that is deliberate rather than lucky.
// Thinker declares its own operator new / operator delete over Doom::host().malloc
// (Sim/Thinker.h says why), so every thinker is still counted here - a plain
// `new Mobj` would have made this measure nothing while still passing. What the
// counter sees besides the thinkers is host-side and deliberately outlives the
// Engine - Host/System's buffer, Host/Sound's audio-blocked paddedsfx, DoomMain's
// response-file argv - so the post-reset figure lands back on the post-boot one.
//
// Measured against the code that first motivated it: 107 blocks after boot, +120 for
// E1M1, and all 227 still outstanding after resetEngine().
auto tResetEngineReleasesTheThinkers =
    test("Engine/resetEngineReleasesTheThinkers") = []
{
    // Before the boot, so every block the engine takes is one this counter saw.
    doomSimCountAllocations();

    check(doomSimBoot() != 0, "the engine booted");
    const auto afterBoot = doomSimLiveAllocations();

    check(doomSimLoadLevel(1, 1, 2) != 0, "E1M1 loaded");
    const auto withLevel = doomSimLiveAllocations();

    // Guards the test itself: if a thinker ever stopped allocating through
    // Doom::host().malloc, the assertion below would pass while measuring nothing at
    // all.
    check(withLevel > afterBoot,
          "loading a level took blocks for its mobjs and thinker specials");

    resetEngine();

    check(doomSimLiveAllocations() <= afterBoot,
          "destroying the Engine returned every block its thinkers took");
};
} // namespace
