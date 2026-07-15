#pragma once

#include "../d_event.h" // event_t

namespace Doom
{
// Heads-up display (messages, chat, level title); hu_stuff.cpp keeps the vanilla
// HU_ names as shims.
void huInit(void);
void huStart(void);
doom_boolean huResponder(event_t* ev);
void huTicker(void);
void huDrawer(void);
char huDequeueChatChar(void);
void huErase(void);
} // namespace Doom
