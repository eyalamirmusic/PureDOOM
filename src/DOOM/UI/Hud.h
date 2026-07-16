#pragma once

#include "../d_event.h" // event_t

namespace Doom
{
// Heads-up display (messages, chat, level title); hu_stuff.cpp keeps the vanilla
// HU_ names as shims.
void huInit();
void huStart();
doom_boolean huResponder(event_t* ev);
void huTicker();
void huDrawer();
char huDequeueChatChar();
void huErase();
} // namespace Doom
