#pragma once

#include "../hu_lib.h" // HudScrollingText

namespace Doom
{
// The HUD message line - the one-line notice that pops up top-left ("you got the shotgun", a
// picked-up key, a chat line). w_message is the scrolling-text widget it is drawn through;
// message_on says a message is currently showing (the widget binds to it), message_counter is the
// tics left before it clears (HU_MSGTIMEOUT on each new message), and message_nottobefuckedwith
// marks a message that a lower-priority one may not overwrite. Doom::hudTicker pulls plr->message into
// the widget, times it out, and the chat responder posts received chat here too.
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5) - the first UI
// cluster, and the first that was already file-local (a `static` inside UI/Hud's namespace rather
// than a loose global), so it was off the global cloud but still process-static state a second
// Engine would share. The vanilla names become references onto these members. The status bar and
// HUD are drawn into screens[0] every tic, which the frame goldens hash, and the demos pick items
// up (setting plr->message), so this is live-golden-covered - byte-identical goldens confirm it.
struct HudMessage
{
    doom_boolean message_on = false; // a message is currently showing
    doom_boolean message_nottobefuckedwith =
        false; // this one outranks a plain message
    HudScrollingText w_message = {}; // the scrolling-text widget it is drawn through
    int message_counter = 0; // tics left before the message clears
};

// The one HudMessage, a view onto the Engine's member - the same pattern as the Game/ and Render/
// clusters (gameFlow(), lighting(), ...).
HudMessage& hudMessage();
} // namespace Doom
