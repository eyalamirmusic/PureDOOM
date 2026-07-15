#pragma once

#include "../doomtype.h" // doom_boolean

namespace Doom
{
// The pending special-command requests. A pause (the pause key, in G_Responder) or a save (menu
// save, in G_SaveGame) cannot be applied inside the event that asks for it, so it is deferred:
// sendpause/sendsave are raised, and next tic G_BuildTiccmd folds the request into the ticcmd it
// builds as a BT_SPECIAL command (BTS_PAUSE / BTS_SAVEGAME) and clears the flag. G_DoLoadLevel
// resets both. g_game's own file-scope state, read by no other file (m_menu carried a dead
// `extern doom_boolean sendpause;` it never used - dropped with this move).
//
// Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5); the vanilla names
// become references onto these members. The headless suite never presses pause or saves (demos
// feed the ticcmd directly), so both stay false throughout - golden-neutral.
struct PendingCommands
{
    doom_boolean sendpause = false; // fold a BTS_PAUSE into next tic's command
    doom_boolean sendsave = false; // fold a BTS_SAVEGAME into next tic's command
};

// The one PendingCommands, a view onto the Engine's member - the same pattern as the other Game/
// clusters (ticcmdInput(), timeDemo(), ...).
PendingCommands& pendingCommands();
} // namespace Doom
