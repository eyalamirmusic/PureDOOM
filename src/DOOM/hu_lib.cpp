// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Heads-up text widgets. Rewritten in UI/HudWidgets.{h,cpp}; this keeps
//        the HUlib_ names as shims. The widgets carry all their state in the
//        caller's structs, so there are no globals to own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "hu_lib.h"

#include "UI/HudWidgets.h"

void HUlib_init(void)
{
    Doom::initWidgets();
}

void HUlib_clearTextLine(hu_textline_t* t)
{
    Doom::clearTextLine(*t);
}

void HUlib_initTextLine(hu_textline_t* t, int x, int y, patch_t** f, int sc)
{
    Doom::initTextLine(*t, x, y, f, sc);
}

doom_boolean HUlib_addCharToTextLine(hu_textline_t* t, char ch)
{
    return Doom::addCharToTextLine(*t, ch);
}

doom_boolean HUlib_delCharFromTextLine(hu_textline_t* t)
{
    return Doom::delCharFromTextLine(*t);
}

void HUlib_drawTextLine(hu_textline_t* l, doom_boolean drawcursor)
{
    Doom::drawTextLine(*l, drawcursor);
}

void HUlib_eraseTextLine(hu_textline_t* l)
{
    Doom::eraseTextLine(*l);
}

void HUlib_initSText(hu_stext_t* s, int x, int y, int h, patch_t** font,
                     int startchar, doom_boolean* on)
{
    Doom::initSText(*s, x, y, h, font, startchar, on);
}

void HUlib_addLineToSText(hu_stext_t* s)
{
    Doom::addLineToSText(*s);
}

void HUlib_addMessageToSText(hu_stext_t* s, const char* prefix, const char* msg)
{
    Doom::addMessageToSText(*s, prefix, msg);
}

void HUlib_drawSText(hu_stext_t* s)
{
    Doom::drawSText(*s);
}

void HUlib_eraseSText(hu_stext_t* s)
{
    Doom::eraseSText(*s);
}

void HUlib_initIText(hu_itext_t* it, int x, int y, patch_t** font, int startchar,
                     doom_boolean* on)
{
    Doom::initIText(*it, x, y, font, startchar, on);
}

void HUlib_delCharFromIText(hu_itext_t* it)
{
    Doom::delCharFromIText(*it);
}

void HUlib_eraseLineFromIText(hu_itext_t* it)
{
    Doom::eraseLineFromIText(*it);
}

void HUlib_resetIText(hu_itext_t* it)
{
    Doom::resetIText(*it);
}

void HUlib_addPrefixToIText(hu_itext_t* it, char* str)
{
    Doom::addPrefixToIText(*it, str);
}

doom_boolean HUlib_keyInIText(hu_itext_t* it, unsigned char ch)
{
    return Doom::keyInIText(*it, ch);
}

void HUlib_drawIText(hu_itext_t* it)
{
    Doom::drawIText(*it);
}

void HUlib_eraseIText(hu_itext_t* it)
{
    Doom::eraseIText(*it);
}
