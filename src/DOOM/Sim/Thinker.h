#pragma once

namespace Doom
{
// The base of everything that acts once a tic: mobjs and the moving-sector
// specials (doors, lifts, crushers, lights). Vanilla threaded these on a doubly
// linked list whose node carried a function pointer - `thinker_t.function` - that
// Doom::runThinkers called each tic, and that p_saveg compared against `Doom::mobjThinker`
// / the `T_*` addresses to tell one kind of thinker from another. That union is
// gone: dispatch is a virtual `tick()`, and the type is a virtual `kind()`.
//
// The two sentinel states the function pointer used to double as are now explicit
// flags on the base:
//   - `removed` replaces `function.acv == (actionf_v) -1`. Removal stays lazy -
//     Doom::removeThinker sets the flag, and Doom::runThinkers unlinks and frees the block
//     when its turn next comes up (a mobj may remove itself mid-think).
//   - `stopped` replaces `function.acv == 0`. A crusher or lift put into stasis
//     (Doom::ceilingCrushStop / Doom::stopPlat) stays on the list but is skipped by
//     Doom::runThinkers until reactivated; vanilla nulled the function to do this.
//
// `kind()` is the append-only discriminator p_saveg and the mobj-finding loops use
// in place of the old function-pointer identity test.
enum class ThinkerKind
{
    None, // the list's sentinel head (thinkercap), never ticked
    Mobj,
    Ceiling,
    Door,
    Floor,
    Plat,
    FireFlicker,
    LightFlash,
    StrobeFlash,
    Glow
};

struct Thinker
{
    Thinker* prev = nullptr;
    Thinker* next = nullptr;

    bool removed = false; // was function.acv == (actionf_v) -1
    bool stopped = false; // was function.acv == 0 (in stasis)

    virtual ~Thinker() = default;

    // What this thinker does each tic. The sentinel head and a stopped special do
    // nothing; Doom::runThinkers skips them without calling this.
    virtual void tick() {}

    virtual ThinkerKind kind() const { return ThinkerKind::None; }
};
} // namespace Doom
