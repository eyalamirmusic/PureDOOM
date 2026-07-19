#pragma once

#include "../Game/GameDefs.h" // MAXPLAYERS
#include "../doomtype.h" // doom_boolean
#include "HudWidgetTypes.h" // HudInputText

namespace Doom
{
// The heads-up chat state (multiplayer talk). w_chat is the input line you type into; the local
// keystrokes queue up in the chatchars ring (head/tail) for the netcode to send. Each remote
// player's incoming text builds in its own w_inputbuffer, and chat_dest[i] holds who player i is
// addressing (a player number, or HU_BROADCAST). always_off is a permanently-false flag the
// input-buffer widgets bind their "on" pointer to - they are assembled from the wire, never shown
// as an active editable line, so their cursor is wired off.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); these were
// UI/Hud's own file-local statics, read by no other file. UI/Hud reaches them through a hoisted
// `auto& chat = hudChat();` local per function rather than a file-scope or function-scope alias
// (REFACTOR.md, Step 9 strand (a)). PureDOOM ships single-player, so no demo drives the chat -
// golden-neutral, and confirmed so. (Doom::hudResponder's send-path state - lastmessage,
// shiftdown/altdown, num_nobrainers - had already folded in from function-local statics; the
// destination-key table stays file-local, being fixed reference data.)
struct HudChat
{
    static constexpr int queueSize =
        128; // QUEUESIZE in UI/Hud: the chatchars ring size

    HudInputText w_chat = {}; // the local input line being typed
    HudInputText w_inputbuffer[MAXPLAYERS] =
        {}; // each remote player's incoming text
    doom_boolean always_off =
        false; // the input buffers' cursor, wired permanently off
    char chat_dest[MAXPLAYERS] = {}; // who each player is addressing

    char chatchars[queueSize] = {}; // outgoing local keystrokes, awaiting send
    int head = 0; // chatchars ring head
    int tail = 0; // chatchars ring tail

    // Doom::hudResponder's own send-path state, folded in from its function-local statics.
    char lastmessage[HU_MAXLINELENGTH + 1] =
        {}; // the last message sent (HU_MSGREFRESH)
    doom_boolean shiftdown = false; // shift held (chat input)
    doom_boolean altdown = false; // alt held (chat input)
    int num_nobrainers = 0; // consecutive "no-brainer" chat macro sends
};

// The one HudChat, a view onto the Engine's member - the same pattern as the other clusters
// (hudMessage(), playerState(), ...).
HudChat& hudChat();
} // namespace Doom
