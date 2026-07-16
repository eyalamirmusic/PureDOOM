#pragma once

#include "../hu_lib.h" // hu_textline_t / hu_stext_t / hu_itext_t, patch_t

namespace Doom
{
// Heads-up text widgets; hu_lib.cpp keeps the vanilla HUlib_ names as shims.
void initWidgets();
void clearTextLine(hu_textline_t& t);
void initTextLine(hu_textline_t& t, int x, int y, patch_t** f, int sc);
doom_boolean addCharToTextLine(hu_textline_t& t, char ch);
doom_boolean delCharFromTextLine(hu_textline_t& t);
void drawTextLine(hu_textline_t& l, doom_boolean drawcursor);
void eraseTextLine(hu_textline_t& l);
void initSText(hu_stext_t& s,
               int x,
               int y,
               int h,
               patch_t** font,
               int startchar,
               doom_boolean* on);
void addLineToSText(hu_stext_t& s);
void addMessageToSText(hu_stext_t& s, const char* prefix, const char* msg);
void drawSText(hu_stext_t& s);
void eraseSText(hu_stext_t& s);
void initIText(
    hu_itext_t& it, int x, int y, patch_t** font, int startchar, doom_boolean* on);
void delCharFromIText(hu_itext_t& it);
void eraseLineFromIText(hu_itext_t& it);
void resetIText(hu_itext_t& it);
void addPrefixToIText(hu_itext_t& it, char* str);
doom_boolean keyInIText(hu_itext_t& it, unsigned char ch);
void drawIText(hu_itext_t& it);
void eraseIText(hu_itext_t& it);
} // namespace Doom
