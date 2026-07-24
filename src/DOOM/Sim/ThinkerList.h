#pragma once

#include "Thinker.h"

namespace Doom
{
// The head of the thinker list - vanilla's thinkercap. Every actor that acts once a
// tic (a mobj, a moving-sector special) is linked onto a circular doubly-linked list
// whose sentinel this is: the constructor (and per-level reset) seeds it pointing at
// itself, addThinker splices
// new thinkers in before it, and runThinkers walks thinkercap.next .. thinkercap,
// ticking each and skipping the sentinel (which is never ticked - its kind() is None).
// SaveGame, Enemy and Render/Data walk the same list; all reach it through p_local.h.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5), now that
// the Thinker rewrite (Step 8) made the node a real type - so the simulation's central
// list is owned per-Engine rather than by the process, a step toward the engine being
// *constructed* rather than booted. It was the p_tick.cpp shim's own global; the vanilla
// name thinkercap becomes a reference onto cap, so every reader resolves unchanged. Live
// simulation-golden-covered - runThinkers walks this every demo tic.
struct ThinkerList
{
    // the circular list's sentinel head; default-constructed as the None-kind Thinker,
    // relinked to point at itself by reset()
    Thinker cap;

    // The constructor establishes the empty-list invariant, so a freshly constructed
    // ThinkerList is already walkable - before this, cap.prev/next were nullptr until
    // the first initThinkers, a window a reset-fresh Engine (resetEngine) would open.
    ThinkerList() { reset(); }

    // Seeds the circular list empty: the sentinel points at itself, so a walk from
    // cap.next round to cap visits nothing. Construction and the per-level reset
    // (initThinkers) both go through here, so a fresh list and an emptied one are the
    // same state - the same split LevelPool draws between its destructor and
    // releaseAll(). The blocks themselves are freed separately, by the LevelPool; this
    // only forgets the list threaded through them.
    void reset() { cap.prev = cap.next = &cap; }
};

// The one ThinkerList, a view onto the Engine's member - the same pattern as the other
// clusters.
ThinkerList& thinkerList();
} // namespace Doom
