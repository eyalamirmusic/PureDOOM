#pragma once

#include "../Game/Event.h" // Event

namespace Doom
{
// Status bar (widgets, face, palette flashes, cheats); st_stuff.cpp keeps the
// vanilla ST_ names as shims.
bool statusBarResponder(Event& ev);
void statusBarTicker();
void drawStatusBar(bool fullscreen, bool refresh);
void startStatusBar();
void initStatusBar();
} // namespace Doom
