#pragma once

#include "../Game/Event.h" // Event

// The heads-up font's character range, and the message widget's placement and
// timeout. Was hu_stuff.h.
namespace Doom
{
constexpr char HU_FONTSTART = '!'; // the first font characters
constexpr char HU_FONTEND = '_'; // the last font characters
constexpr int HU_FONTSIZE = HU_FONTEND - HU_FONTSTART + 1;

constexpr int HU_BROADCAST = 5;
constexpr int HU_MSGREFRESH = KEY_ENTER;
constexpr int HU_MSGX = 0;
constexpr int HU_MSGY = 0;
constexpr int HU_MSGHEIGHT = 1; // in lines
constexpr int HU_MSGTIMEOUT = 4 * TICRATE;
} // namespace Doom

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
