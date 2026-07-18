#pragma once

#include "../d_event.h" // event_t, doom_boolean

namespace Doom
{
// The DOOM menu: the main menu, options and their toggles, the episode/skill,
// save/load and sound submenus, the yes/no prompts and the title-screen F-keys.
// m_menu.cpp keeps the vanilla M_ names as shims over these five entry points.
//
// Everything else the menu owns - the menu-definition tables, the skull cursor,
// the message and save-string state - is file-local to Menu.cpp; only the few
// globals other subsystems read (menuactive, the config-backed screenblocks /
// detailLevel / showMessages / mouseSensitivity, inhelpscreens, messageToPrint)
// stay at file scope for them. The M_ names are kept inside the namespace so the
// transcription stays diffable against the 1993 source.
doom_boolean menuResponder(event_t* ev);
void menuTicker();
void drawMenu();
void initMenu();
void startControlPanel();
} // namespace Doom
