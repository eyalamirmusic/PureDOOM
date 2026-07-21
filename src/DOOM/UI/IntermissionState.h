#pragma once

#include "../Game/PlayerTypes.h" // IntermissionStart, IntermissionPlayer
#include "../Game/GameDefs.h" // MAXPLAYERS
#include "../Sim/MapTypes.h"
#include "../Render/RenderTypes.h" // Patch
#include "IntermissionTypes.h" // IntermissionPhase

#include "../Containers.h"

namespace Doom
{
// The level-completion intermission's residual runtime state and loaded graphics - what
// UI/Intermission keeps for itself as the single-player count-up, the coop/deathmatch tables and
// the "you are here" pointer play out. Three loosely-related threads: the timing/state machine
// (the current IntermissionPhase `state`, the `acceleratestage` skip flag, the `cnt`/`bcnt` counters, the
// per-stage `dm_state`/`ng_state`/`sp_state`, and the animated count-up accumulators `cnt_*`), the
// passed-in scoreboard data (`wbs`/`plrs`/`me`), and the patches `loadIntermissionData` reads from the WAD
// once and the drawers then paint from (`bg`, `num`, `kills`, the player faces, ...) plus the
// `lnames` pointer array.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were all
// UI/Intermission's own file-local statics (internal linkage, read by no other file). They were all
// reached through file-scope reference aliases (the arrays as references-to-array) until the
// file-local-alias sweep (REFACTOR.md, Step 9 strand (a)) retired them - every function in
// UI/Intermission now hoists a single `auto& im = intermissionState();` and reaches its members
// through it. No attract demo or menu replay reaches the intermission, so this is not
// golden-covered, but the hoist is a mechanical per-function rewrite (the compiler still binds
// every access to the same member), so it is behaviour-preserving by construction, verified by
// build + app-link.
//
// The animation/layout data tables stay file-local in UI/Intermission and do *not* move in here:
// `anims_wi_stuff` is a table of pointers *into* `epsd0/1/2animinfo`, the same self-referential-
// pointer trap AutomapView's "iddt" cheat sequence hit (a copyable struct member holding a pointer
// to its own storage does not survive being copied), so that whole animation-data group - the
// const `lnodes`/`NUMANIMS` alongside the animation-state arrays it indexes - is left for a later
// pass.
//
// No firstrefresh: set to 1 by startIntermission ("refresh everything for one
// frame") and tested nowhere, in this rewrite or in vanilla wi_stuff.c. Verified
// against the 1993 source in this repository's history; deleted rather than
// carried, as no read was lost.
struct IntermissionState
{
    // Timing and the count-up state machine.
    int acceleratestage = 0; // skip the current stage's delay
    IntermissionPhase state = IntermissionPhase::
        StatCount; // zero-init lands here (IntermissionPhase::NoState is -1); reset by wiInit* before use
    int cnt = 0; // general timing
    int bcnt = 0; // background-animation timing
    int dm_state = 0; // deathmatch table sub-state
    int ng_state = 0; // netgame table sub-state
    int sp_state = 0; // single-player table sub-state
    int cnt_pause = 0; // inter-stage pause counter

    // The passed-in scoreboard data.
    int me = 0; // wbs->pnum, the player being shown
    IntermissionStart* wbs = nullptr; // the intermission parameters
    IntermissionPlayer* plrs = nullptr; // wbs->plyr[]

    // The animated count-up accumulators.
    Array<int, MAXPLAYERS> cnt_kills = {};
    Array<int, MAXPLAYERS> cnt_items = {};
    Array<int, MAXPLAYERS> cnt_secret = {};
    int cnt_time = 0;
    int cnt_par = 0;
    Array<int, MAXPLAYERS> cnt_frags = {};

    // Deathmatch tallies and the frag-column flag.
    bool snl_pointeron = false; // the "you are here" pointer is lit this frame
    Array<Array<int, MAXPLAYERS>, MAXPLAYERS> dm_frags = {};
    Array<int, MAXPLAYERS> dm_totals = {};
    int dofrags = 0; // netgame has frags to show

    int NUMCMAPS = 0; // # of commercial levels (set in loadIntermissionData)

    // Graphics loaded once by loadIntermissionData, read-only after.
    Patch* bg = nullptr; // background (map of levels)
    Array<Patch*, 2> yah = {}; // "you are here" (two blink frames)
    Patch* splat = nullptr; // splat on completed levels
    Patch* percent = nullptr; // %
    Patch* colon = nullptr; // :
    Array<Patch*, 10> num = {}; // 0-9
    Patch* wiminus = nullptr; // minus sign
    Patch* finished = nullptr; // "Finished!"
    Patch* entering = nullptr; // "Entering"
    Patch* sp_secret = nullptr; // "secret"
    Patch* kills = nullptr; // "Kills"
    Patch* secret = nullptr; // "Scrt"
    Patch* items = nullptr; // "Items"
    Patch* frags = nullptr; // "Frags"
    Patch* time_patch = nullptr; // "Time"
    Patch* par = nullptr; // "Par"
    Patch* sucks = nullptr; // "sucks" (time overflow)
    Patch* killers = nullptr; // "killers" (vertical)
    Patch* victims = nullptr; // "victims" (horizontal)
    Patch* total = nullptr; // "Total"
    Patch* star = nullptr; // your face
    Patch* bstar = nullptr; // your dead face
    Array<Patch*, MAXPLAYERS> p = {}; // "red P[1..MAXPLAYERS]"
    Array<Patch*, MAXPLAYERS> bp = {}; // "gray P[1..MAXPLAYERS]"

    // Per-level name graphics, sized and filled by loadIntermissionData. RAII-owned
    // (Step 9): what was a raw doom_malloc'd Patch** is now an owning Vector of
    // pointers - read only within Intermission.cpp, so unlike wipe_melt_offsets this
    // needs no separate view onto it. The pointed-to Patch lumps themselves are owned
    // by the WAD (Doom::WadFile), not by this vector, and must not be freed here.
    Vector<Patch*> lnames;
};

// The one IntermissionState, a view onto the Engine's member - the same pattern as the other
// clusters (automapView(), statusBarState(), ...).
IntermissionState& intermissionState();
} // namespace Doom
