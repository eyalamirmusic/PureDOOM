#pragma once

#include "../doomtype.h" // doom_boolean
#include "../st_stuff.h" // st_stateenum_t, st_chatstateenum_t

// player_t is used only by pointer here, so a forward declaration is enough.
typedef struct player_s player_t;

namespace Doom
{
// The status bar's residual runtime state - what is left of UI/StatusBar's own file-scope statics
// once the animated face, the STlib widgets and the loaded patches have moved out (StatusBarFace /
// StatusBarWidgets / StatusBarGraphics). Four loosely-related threads the bar keeps for itself: the
// lifecycle/timing bookkeeping, which layout is drawn, the "t"-to-talk chat line, and the palette
// flash. None is read by any other file.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); the vanilla names
// become references onto these members (keyboxes as a reference-to-array). ST_Start / ST_initData
// reset most of these before use, so the defaults matter little, but they reproduce vanilla's
// zero/one/true initializers exactly. The bar is drawn into screens[0] every tic, so this is
// frame-golden-covered.
struct StatusBarState
{
    // Lifecycle and timing.
    player_t* plyr = nullptr; // the console player, refreshed at ST_Start
    doom_boolean st_firsttime = false; // ST_Start just ran (force a full redraw)
    int veryfirsttime = 1; // gate for the one-time ST_initData
    int lu_palette = 0; // the PLAYPAL lump number
    unsigned int st_clock = 0; // the tic clock the face animation counts on
    int st_msgcounter = 0; // tics a status-bar chat message stays up
    doom_boolean st_stopped = true; // ST_Stop has parked the bar

    // Which layout is drawn, and the keys shown.
    st_stateenum_t st_gamestate = {}; // automap vs first-person (0 = AutomapState)
    doom_boolean st_notdeathmatch = false; // single-player layout (arms, not frags)
    doom_boolean st_armson = false; // the arms panel is shown (not DM, bar up)
    doom_boolean st_fragson = false; // the frags summary is shown (deathmatch)
    int st_fragscount = 0; // the frags total w_frags draws
    int keyboxes[3] = {}; // the key-type shown in each of the three key slots

    // The status-bar chat line (the "t"-to-talk entry).
    st_chatstateenum_t st_chatstate =
        {}; // where in chat entry we are (0 = StartChatState)
    doom_boolean st_chat = false; // chat entry is active
    doom_boolean st_oldchat = false; // st_chat before a message popped up
    doom_boolean st_cursoron = false; // the chat cursor is shown

    // The palette flash (pain red, pickup gold, radsuit green).
    int st_palette = 0; // the PLAYPAL sub-palette currently set
};

// The one StatusBarState, a view onto the Engine's member - the same pattern as the other clusters
// (statusBarWidgets(), hudMessage(), ...).
StatusBarState& statusBarState();
} // namespace Doom
