// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:  none
//
//-----------------------------------------------------------------------------

#pragma once

// We are referring to patches.
#include "../Sim/MapTypes.h"
#include "../Render/RenderTypes.h"

#include "../Containers.h"

#include <string>
#include <string_view>

// background and foreground screen numbers
// different from other modules.
namespace Doom
{
constexpr int FG = 0;

constexpr int HU_MAXLINES = 4;
constexpr int HU_MAXLINELENGTH = 80;

//
// Typedefs of widgets
//

// Text Line widget
//  (parent of Scrolling Text and Input Text widgets)
struct HudTextLine
{
    // left-justified position of scrolling text window
    int x = 0;
    int y = 0;

    Patch** f = nullptr; // font
    int sc = 0; // start character
    std::string l; // line of text

    // whether this line needs to be udpated
    int needsupdate = 0;

    // The vanilla HUlib_*TextLine family, each keyed off one line, so each is a
    // method. Bodies in UI/HudWidgets.cpp.
    void init(int xToUse, int yToUse, Patch** fToUse, int scToUse);
    void clear();
    bool addChar(char ch);
    bool delChar();
    void draw(bool drawcursor);
    void erase();
};

// Scrolling Text window widget
//  (child of Text Line widget)
struct HudScrollingText
{
    Array<HudTextLine, HU_MAXLINES> l; // text lines to draw
    int h = 0; // height in lines
    int cl = 0; // current line number

    // pointer to bool stating whether to update window
    bool* on = nullptr;
    bool laston = false; // last value of *->on.

    // The vanilla HUlib_*SText family. Bodies in UI/HudWidgets.cpp.
    void init(int x, int y, int hToUse, Patch** font, int startchar, bool* onToUse);
    void addLine();
    void addMessage(std::string_view prefix, std::string_view msg);
    void draw();
    void erase();
};

// Input Text Line widget
//  (child of Text Line widget)
struct HudInputText
{
    HudTextLine l; // text line to input on

    // left margin past which I am not to delete characters
    int lm = 0;

    // pointer to bool stating whether to update window
    bool* on = nullptr;
    bool laston = false; // last value of *->on;

    // The vanilla HUlib_*IText family. Bodies in UI/HudWidgets.cpp.
    void init(int x, int y, Patch** font, int startchar, bool* onToUse);
    void reset();
    void delChar();
    bool keyIn(unsigned char ch);
    void draw();
    void erase();
};
} // namespace Doom

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
