#pragma once

#include "../d_event.h" // Event

namespace Doom
{
// Heads-up display (messages, chat, level title); hu_stuff.cpp keeps the vanilla
// HU_ names as shims.
void initHud();
void startHud();
doom_boolean hudResponder(Event* ev);
void hudTicker();
void drawHud();
char dequeueChatChar();
void eraseHud();
} // namespace Doom
