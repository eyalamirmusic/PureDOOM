#pragma once

#include "../Game/Event.h" // Event

// The heads-up font's character range, and the message widget's placement and
// timeout. Was hu_stuff.h.
#define HU_FONTSTART '!' // the first font characters
#define HU_FONTEND '_' // the last font characters
#define HU_FONTSIZE (HU_FONTEND - HU_FONTSTART + 1)

#define HU_BROADCAST 5
#define HU_MSGREFRESH KEY_ENTER
#define HU_MSGX 0
#define HU_MSGY 0
#define HU_MSGWIDTH 64 // in characters
#define HU_MSGHEIGHT 1 // in lines
#define HU_MSGTIMEOUT (4 * TICRATE)

namespace Doom
{
// Heads-up display (messages, chat, level title); hu_stuff.cpp keeps the vanilla
// HU_ names as shims.
void initHud();
void startHud();
bool hudResponder(Event* ev);
void hudTicker();
void drawHud();
char dequeueChatChar();
void eraseHud();
} // namespace Doom
