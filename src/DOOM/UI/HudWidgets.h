#pragma once

#include "HudWidgetTypes.h" // HudTextLine / HudScrollingText / HudInputText, Patch

namespace Doom
{
// Heads-up text widgets. The vanilla HUlib_ names that used to shim these
// have been retired; call sites use these Doom:: names directly.
void initWidgets();
void clearTextLine(HudTextLine& t);
void initTextLine(HudTextLine& t, int x, int y, Patch** f, int sc);
doom_boolean addCharToTextLine(HudTextLine& t, char ch);
doom_boolean delCharFromTextLine(HudTextLine& t);
void drawTextLine(HudTextLine& l, doom_boolean drawcursor);
void eraseTextLine(HudTextLine& l);
void initSText(HudScrollingText& s,
               int x,
               int y,
               int h,
               Patch** font,
               int startchar,
               doom_boolean* on);
void addLineToSText(HudScrollingText& s);
void addMessageToSText(HudScrollingText& s, const char* prefix, const char* msg);
void drawSText(HudScrollingText& s);
void eraseSText(HudScrollingText& s);
void initIText(
    HudInputText& it, int x, int y, Patch** font, int startchar, doom_boolean* on);
void delCharFromIText(HudInputText& it);
void eraseLineFromIText(HudInputText& it);
void resetIText(HudInputText& it);
void addPrefixToIText(HudInputText& it, char* str);
doom_boolean keyInIText(HudInputText& it, unsigned char ch);
void drawIText(HudInputText& it);
void eraseIText(HudInputText& it);
} // namespace Doom
