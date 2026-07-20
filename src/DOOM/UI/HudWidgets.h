#pragma once

#include "HudWidgetTypes.h" // HudTextLine / HudScrollingText / HudInputText, Patch

#include <string_view>

namespace Doom
{
// Heads-up text widgets. The vanilla HUlib_ names that used to shim these
// have been retired; call sites use these Doom:: names directly.
void initWidgets();
void clearTextLine(HudTextLine& t);
void initTextLine(HudTextLine& t, int x, int y, Patch** f, int sc);
bool addCharToTextLine(HudTextLine& t, char ch);
bool delCharFromTextLine(HudTextLine& t);
void drawTextLine(HudTextLine& l, bool drawcursor);
void eraseTextLine(HudTextLine& l);
void initSText(
    HudScrollingText& s, int x, int y, int h, Patch** font, int startchar, bool* on);
void addLineToSText(HudScrollingText& s);
void addMessageToSText(HudScrollingText& s,
                       std::string_view prefix,
                       std::string_view msg);
void drawSText(HudScrollingText& s);
void eraseSText(HudScrollingText& s);
void initIText(
    HudInputText& it, int x, int y, Patch** font, int startchar, bool* on);
void delCharFromIText(HudInputText& it);
void eraseLineFromIText(HudInputText& it);
void resetIText(HudInputText& it);
void addPrefixToIText(HudInputText& it, std::string_view str);
bool keyInIText(HudInputText& it, unsigned char ch);
void drawIText(HudInputText& it);
void eraseIText(HudInputText& it);
} // namespace Doom
