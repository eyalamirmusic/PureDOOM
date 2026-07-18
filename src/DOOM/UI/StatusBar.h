#pragma once

#include "../d_event.h" // event_t

namespace Doom
{
// Status bar (widgets, face, palette flashes, cheats); st_stuff.cpp keeps the
// vanilla ST_ names as shims.
doom_boolean statusBarResponder(event_t* ev);
void statusBarTicker();
void drawStatusBar(doom_boolean fullscreen, doom_boolean refresh);
void startStatusBar();
void initStatusBar();
} // namespace Doom
