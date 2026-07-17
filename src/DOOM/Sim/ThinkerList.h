#pragma once

#include "Thinker.h"

namespace Doom
{
// The head of the thinker list - vanilla's thinkercap. Every actor that acts once a
// tic (a mobj, a moving-sector special) is linked onto a circular doubly-linked list
// whose sentinel this is: initThinkers seeds it pointing at itself, addThinker splices
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
    // relinked to point at itself by initThinkers
    Thinker cap;
};

// The one ThinkerList, a view onto the Engine's member - the same pattern as the other
// clusters.
ThinkerList& thinkerList();
} // namespace Doom
