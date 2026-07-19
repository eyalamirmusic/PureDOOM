#pragma once

#include "../Sim/Info.h" // State

namespace Doom
{
// The end-of-episode finale's runtime state - what UI/Finale keeps as the scrolling text screen,
// the art/credit screen and the DOOM II character cast call play out: which stage the animation is
// in and how long it has run, the chosen text and background flat for this ending, and the cast
// call's per-monster bookkeeping (which monster, its animation state into the global states[], the
// death/attack flags and the frame counters).
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// UI/Finale's own namespace-scope globals, read by no other file. The vanilla names were
// references onto the members until the file-local-alias sweep (REFACTOR.md, Step 9 strand (a))
// retired them; UI/Finale.cpp now reaches finaleState() through a hoisted local per function
// instead. No attract demo or menu replay reaches a finale, but it has its own frame golden now
// (Tests/Goldens/finale.frames, via Tests/FinaleReplay.h), which is what made retiring these
// aliases safe to verify by more than build + app-link.
//
// The finale's immutable reference data stays file-local in UI/Finale and does *not* move in here:
// the per-ending text pointers (e1text..t6text, each aliasing a string-literal macro) and the
// castorder[] cast list are fixed constants, not per-run state - a second Engine sharing them is
// harmless - and there is no self-reference to unwind (caststate points into the global states[],
// not into any of these). Migrating 22 literal-pointer aliases and the cast table would be bulk for
// no gain, so they are left as they are.
struct FinaleState
{
    int finalestage = 0; // 0 = text, 1 = art screen, 2 = character cast
    int finalecount = 0; // tics since the stage began

    const char* finaletext = nullptr; // the scrolling text for this ending
    const char* finaleflat = nullptr; // the tiled background flat

    // The DOOM II cast call.
    int castnum = 0; // which monster in castorder is on screen
    int casttics = 0; // tics left in the current cast state
    State* caststate = nullptr; // current animation state (into the global states[])
    bool castdeath = false; // the shown monster is dying
    int castframes = 0; // frames shown in the current cycle
    int castonmelee = 0; // alternate melee/missile attack toggle
    bool castattacking = false; // in an attack frame

    // The bunny-scroll ending's animation cursor (was a function-local static in
    // F_BunnyScroll), driving the "end." stamp that appears when scrolling finishes.
    int laststage = 0;
};

// The one FinaleState, a view onto the Engine's member - the same pattern as the other clusters
// (intermissionState(), automapView(), ...).
FinaleState& finaleState();
} // namespace Doom
