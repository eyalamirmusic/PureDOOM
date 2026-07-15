#pragma once

#include "../d_player.h" // wbstartstruct_t, wbplayerstruct_t
#include "../doomdef.h" // MAXPLAYERS
#include "../doomtype.h" // doom_boolean
#include "../r_defs.h" // patch_t
#include "../wi_stuff.h" // stateenum_t

namespace Doom
{
// The level-completion intermission's residual runtime state and loaded graphics - what
// UI/Intermission keeps for itself as the single-player count-up, the coop/deathmatch tables and
// the "you are here" pointer play out. Three loosely-related threads: the timing/state machine
// (the current stateenum_t `state`, the `acceleratestage` skip flag, the `cnt`/`bcnt` counters, the
// per-stage `dm_state`/`ng_state`/`sp_state`, and the animated count-up accumulators `cnt_*`), the
// passed-in scoreboard data (`wbs`/`plrs`/`me`), and the patches `wiLoadData` reads from the WAD
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
    stateenum_t state =
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
    wbstartstruct_t* wbs = nullptr; // the intermission parameters
    wbplayerstruct_t* plrs = nullptr; // wbs->plyr[]

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

    int NUMCMAPS = 0; // # of commercial levels (set in wiLoadData)

    // Graphics loaded once by wiLoadData, read-only after.
    patch_t* bg = nullptr; // background (map of levels)
    patch_t* yah[2] = {}; // "you are here" (two blink frames)
    patch_t* splat = nullptr; // splat on completed levels
    patch_t* percent = nullptr; // %
    patch_t* colon = nullptr; // :
    patch_t* num[10] = {}; // 0-9
    patch_t* wiminus = nullptr; // minus sign
    patch_t* finished = nullptr; // "Finished!"
    patch_t* entering = nullptr; // "Entering"
    patch_t* sp_secret = nullptr; // "secret"
    patch_t* kills = nullptr; // "Kills"
    patch_t* secret = nullptr; // "Scrt"
    patch_t* items = nullptr; // "Items"
    patch_t* frags = nullptr; // "Frags"
    patch_t* time_patch = nullptr; // "Time"
    patch_t* par = nullptr; // "Par"
    patch_t* sucks = nullptr; // "sucks" (time overflow)
    patch_t* killers = nullptr; // "killers" (vertical)
    patch_t* victims = nullptr; // "victims" (horizontal)
    patch_t* total = nullptr; // "Total"
    patch_t* star = nullptr; // your face
    patch_t* bstar = nullptr; // your dead face
    patch_t* p[MAXPLAYERS] = {}; // "red P[1..MAXPLAYERS]"
    patch_t* bp[MAXPLAYERS] = {}; // "gray P[1..MAXPLAYERS]"
    patch_t** lnames = nullptr; // per-level name graphics (malloc'd by wiLoadData)
};

// The one IntermissionState, a view onto the Engine's member - the same pattern as the other
// clusters (automapView(), statusBarState(), ...).
IntermissionState& intermissionState();
} // namespace Doom
