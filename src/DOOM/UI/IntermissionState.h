#pragma once

#include "../Game/PlayerTypes.h" // IntermissionStart, IntermissionPlayer
#include "../doomdef.h" // MAXPLAYERS
#include "../doomtype.h" // doom_boolean
#include "../r_defs.h" // Patch
#include "IntermissionTypes.h" // IntermissionPhase

namespace Doom
{
// The level-completion intermission's residual runtime state and loaded graphics - what
// UI/Intermission keeps for itself as the single-player count-up, the coop/deathmatch tables and
// the "you are here" pointer play out. Three loosely-related threads: the timing/state machine
// (the current IntermissionPhase `state`, the `acceleratestage` skip flag, the `cnt`/`bcnt` counters, the
// per-stage `dm_state`/`ng_state`/`sp_state`, and the animated count-up accumulators `cnt_*`), the
// passed-in scoreboard data (`wbs`/`plrs`/`me`), and the patches `loadIntermissionData` reads from the WAD
// once and the drawers then paint from (`bg`, `num`, `kills`, the player faces, ...) plus the
// malloc'd `lnames` pointer array.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were all
// UI/Intermission's own file-local statics (internal linkage, read by no other file). The vanilla
// names become references onto the members (the arrays as references-to-array). No attract demo or
// menu replay reaches the intermission, so this is not golden-covered - but a reference alias is
// pure storage relocation (the compiler binds each name to its same-named member), so it is
// behaviour-preserving by construction, verified by build + app-link.
//
// The animation/layout data tables stay file-local in UI/Intermission and do *not* move in here:
// `anims_wi_stuff` is a table of pointers *into* `epsd0/1/2animinfo`, the same self-referential-
// pointer trap AutomapView's "iddt" cheat sequence hit (a copyable struct member holding a pointer
// to its own storage does not survive being copied), so that whole animation-data group - the
// const `lnodes`/`NUMANIMS` alongside the animation-state arrays it indexes - is left for a later
// pass.
struct IntermissionState
{
    // Timing and the count-up state machine.
    int acceleratestage = 0; // skip the current stage's delay
    IntermissionPhase state =
        StatCount; // zero-init lands here (NoState is -1); reset by wiInit* before use
    int cnt = 0; // general timing
    int bcnt = 0; // background-animation timing
    int firstrefresh = 0; // refresh everything for one frame
    int dm_state = 0; // deathmatch table sub-state
    int ng_state = 0; // netgame table sub-state
    int sp_state = 0; // single-player table sub-state
    int cnt_pause = 0; // inter-stage pause counter

    // The passed-in scoreboard data.
    int me = 0; // wbs->pnum, the player being shown
    IntermissionStart* wbs = nullptr; // the intermission parameters
    IntermissionPlayer* plrs = nullptr; // wbs->plyr[]

    // The animated count-up accumulators.
    int cnt_kills[MAXPLAYERS] = {};
    int cnt_items[MAXPLAYERS] = {};
    int cnt_secret[MAXPLAYERS] = {};
    int cnt_time = 0;
    int cnt_par = 0;
    int cnt_frags[MAXPLAYERS] = {};

    // Deathmatch tallies and the frag-column flag.
    doom_boolean snl_pointeron =
        false; // the "you are here" pointer is lit this frame
    int dm_frags[MAXPLAYERS][MAXPLAYERS] = {};
    int dm_totals[MAXPLAYERS] = {};
    int dofrags = 0; // netgame has frags to show

    int NUMCMAPS = 0; // # of commercial levels (set in loadIntermissionData)

    // Graphics loaded once by loadIntermissionData, read-only after.
    Patch* bg = nullptr; // background (map of levels)
    Patch* yah[2] = {}; // "you are here" (two blink frames)
    Patch* splat = nullptr; // splat on completed levels
    Patch* percent = nullptr; // %
    Patch* colon = nullptr; // :
    Patch* num[10] = {}; // 0-9
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
    Patch* p[MAXPLAYERS] = {}; // "red P[1..MAXPLAYERS]"
    Patch* bp[MAXPLAYERS] = {}; // "gray P[1..MAXPLAYERS]"
    Patch** lnames =
        nullptr; // per-level name graphics (malloc'd by loadIntermissionData)
};

// The one IntermissionState, a view onto the Engine's member - the same pattern as the other
// clusters (automapView(), statusBarState(), ...).
IntermissionState& intermissionState();
} // namespace Doom
