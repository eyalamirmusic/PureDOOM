#pragma once

#include "../Containers.h"

// Doom::Player is used only by pointer here, so a forward declaration is enough.
namespace Doom
{
struct Player;
} // namespace Doom

namespace Doom
{
// The status bar's residual runtime state - what is left of UI/StatusBar's own file-scope statics
// once the animated face, the STlib widgets and the loaded patches have moved out (StatusBarFace /
// StatusBarWidgets / StatusBarGraphics). Three loosely-related threads the bar keeps for itself: the
// lifecycle/timing bookkeeping, which layout is drawn, and the palette flash. (There was a fourth,
// the "t"-to-talk chat line; it turned out to be dead in vanilla too - see below.) None is read by
// any other file.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were reached
// through file-scope reference aliases (keyboxes as a reference-to-array) until the file-local-alias
// sweep (REFACTOR.md, Step 9 strand (a)) retired them - every function in UI/StatusBar that touches
// more than one of these members now hoists a single `auto& bar = statusBarState();` and reaches
// them through it (a function touching only one keeps the inline `statusBarState().member` call).
// Doom::startStatusBar / ST_initData reset most of these before use, so the defaults matter little,
// but they reproduce vanilla's zero/one/true initializers exactly. The bar is drawn into screens[0]
// every tic, so this is frame-golden-covered.
//
// Eight members were dropped as verified 1993 leftovers - write-only in both this
// rewrite and vanilla st_stuff.c: veryfirsttime (the one-time ST_initData gate -
// ST_Start already runs unconditionally), st_clock (a tic counter the face
// animation never reads), st_gamestate (automap-vs-first-person, set by the
// responder and initData and read nowhere), st_chatstate (set once to a
// StartChatState and never advanced or read), st_cursoron (the chat cursor flag,
// set once and never drawn from), and the whole three-member chat-popup chain
// below. Verified against the source in this repository's history; deleted rather
// than carried, as no read was lost. The two enums those middle members were
// typed by - StatusBarMode and ChatState - outlived them as declarations nothing
// held, and have since been deleted for the same reason.
//
// That chain is worth recording, because it is why one sweep is not enough. The
// status bar's chat popup was st_msgcounter -> st_chat -> st_oldchat: updateWidgets
// counted the timer down with `if (!--st_msgcounter) st_chat = st_oldchat;`. Only
// st_chat looked write-only to the audit. Deleting it removed st_msgcounter's only
// *read* and st_oldchat's only read at once, so both became write-only in turn -
// and a third pass found nothing left. **Dead state cascades**: a member can be
// alive solely because a dead member reads it, so the sweep has to be re-run to a
// fixpoint rather than trusted after one pass.
//
// The chain was equally dead in vanilla, which is what makes deleting it safe
// rather than a behaviour change: st_msgcounter is declared `= 0` at
// st_stuff.c:249 and *only ever decremented* - nothing anywhere sets it to a
// positive value - so `!--st_msgcounter` came true once every 2^32 tics in 1993
// too. The popup this chain implements has never actually run.
struct StatusBarState
{
    // Lifecycle and timing.
    Player* plyr = nullptr; // the console player, refreshed at Doom::startStatusBar
    bool st_firsttime = false; // Doom::startStatusBar just ran (force a full redraw)
    int lu_palette = 0; // the PLAYPAL lump number
    bool st_stopped = true; // ST_Stop has parked the bar

    // Which layout is drawn, and the keys shown.
    bool st_statusbaron = false; // the main bar is drawn (false = full-screen,
    // no bar). The app reads it to know whether to
    // composite the bar strip, and the STlib widgets
    // bind their "on" pointer to it.
    bool st_notdeathmatch = false; // single-player layout (arms, not frags)
    bool st_armson = false; // the arms panel is shown (not DM, bar up)
    bool st_fragson = false; // the frags summary is shown (deathmatch)
    int st_fragscount = 0; // the frags total w_frags draws
    Array<int, 3> keyboxes = {}; // the key-type shown in each of the three key slots

    // The palette flash (pain red, pickup gold, radsuit green).
    int st_palette = 0; // the PLAYPAL sub-palette currently set
};

// The one StatusBarState, a view onto the Engine's member - the same pattern as the other clusters
// (statusBarWidgets(), hudMessage(), ...).
StatusBarState& statusBarState();
} // namespace Doom
