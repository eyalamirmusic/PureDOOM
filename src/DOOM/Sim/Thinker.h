#pragma once

#include <cstddef>

namespace Doom
{
// The base of everything that acts once a tic: mobjs and the moving-sector
// specials (doors, lifts, crushers, lights). Vanilla threaded these on a doubly
// linked list whose node carried a function pointer - `Thinker.function` - that
// runThinkers called each tic, and that p_saveg compared against `mobjThinker`
// / the `T_*` addresses to tell one kind of thinker from another. That union is
// gone: dispatch is a virtual `tick()`, and the type is a virtual `kind()`.
//
// The list is gone too. `prev`/`next` lived here until the ThinkerList became an
// OwnedVector<Thinker>: nothing ever walked them backwards, and nothing ever
// unlinked from the middle - removal is the `removed` flag below, and the only
// unlink ever performed was at runThinkers' own cursor. A doubly-linked list buys
// O(1) middle-erase and nothing else, so it was buying nothing. Do not add a link
// field back without a reader for it.
//
// The two sentinel states the function pointer used to double as are explicit
// flags:
//   - `removed` replaces `function.acv == (actionf_v) -1`. Removal stays lazy -
//     removeThinker sets the flag, and runThinkers erases the vector slot (which
//     frees the object) when its turn next comes up, so a mobj may remove itself
//     mid-think.
//   - `stopped` replaces `function.acv == 0`. A crusher or lift put into stasis
//     (ceilingCrushStop / stopPlat) stays in the list but is skipped by
//     runThinkers until reactivated; vanilla nulled the function to do this.
//
// There is no type tag. `ThinkerKind`, the enum that replaced the function-pointer
// identity test, was asked two entirely different questions and answered neither
// well, so both are asked directly now:
//
//   - "is this a Mobj?", which is every scan in Sim/Enemy, Sim/Teleport,
//     Render/Data, Sim/SaveGame, the eacp port and the test probe. That is a
//     downcast, and `asMobj()` below *is* the downcast - it hands back the Mobj or
//     nothing, so no caller can test the tag and then cast to the wrong type. It is a
//     plain virtual rather than a dynamic_cast because emitSprites asks it for every
//     thinker on every frame.
//   - "which special is this?", which only Sim/SaveGame's archiveSpecials asks, once
//     per save, because it needs the *static* type to size the record it memcpy's.
//     That one dynamic_casts, and says why at its site.
//
// The tag's real cost was that a `kind()` and a `static_cast` next to it are two
// independent statements of one type, and the compiler checks neither against the
// other. Both forms above make the type impossible to state twice.
struct Mobj;

struct Thinker
{
    bool removed = false; // was function.acv == (actionf_v) -1
    bool stopped = false; // was function.acv == 0 (in stasis)

    virtual ~Thinker() = default;

    // What this thinker does each tic. A stopped special does nothing; runThinkers
    // skips it without calling this.
    virtual void tick() = 0;

    // This thinker as a Mobj, or null if it is one of the specials. Mobj overrides it
    // with `return this`; nothing else overrides it at all.
    virtual Mobj* asMobj() { return nullptr; }

    // Every thinker allocates through the embedder's hook rather than the global
    // operator new, which is what `levelAlloc` was for before the ThinkerList owned
    // these: DOOM.h lets a host replace malloc/free (Doom::host().malloc), and the
    // suite's leak test (Tests/Sim/OwnershipTests.cpp) counts blocks through exactly
    // that pair. A plain `new Mobj` would be invisible to both. Declared here and
    // defined in Sim/Tick.cpp so this header stays free of the Host include; the
    // derived types inherit them, and the virtual destructor is what makes `delete`
    // through a Thinker* find them.
    static void* operator new(std::size_t size);
    static void operator delete(void* block);
};
} // namespace Doom
