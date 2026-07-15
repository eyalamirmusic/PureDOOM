#pragma once

#include "../doomdef.h" // MAXPLAYERS
#include "../doomtype.h" // doom_boolean
#include "../hu_lib.h" // hu_itext_t

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
// UI/Hud's own file-local statics, read by no other file. The vanilla names become references
// onto the members (the arrays as references-to-array). PureDOOM ships single-player, so no demo
// drives the chat - golden-neutral, and confirmed so. (HU_Responder's chat send path keeps a few
// function-local statics - lastmessage, the destination-key table - which a later function-local
// pass will take; this cluster is the file-scope half.)
struct HudChat
{
    static constexpr int queueSize =
        128; // QUEUESIZE in UI/Hud: the chatchars ring size

    hu_itext_t w_chat = {}; // the local input line being typed
    hu_itext_t w_inputbuffer[MAXPLAYERS] = {}; // each remote player's incoming text
    doom_boolean always_off =
        false; // the input buffers' cursor, wired permanently off
    char chat_dest[MAXPLAYERS] = {}; // who each player is addressing

    char chatchars[queueSize] = {}; // outgoing local keystrokes, awaiting send
    int head = 0; // chatchars ring head
    int tail = 0; // chatchars ring tail
};

// The one HudChat, a view onto the Engine's member - the same pattern as the other clusters
// (hudMessage(), playerState(), ...).
HudChat& hudChat();
} // namespace Doom
