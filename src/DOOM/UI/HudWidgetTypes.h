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


// background and foreground screen numbers
// different from other modules.
#define BG 1
#define FG 0

// font stuff
#define HU_CHARERASE KEY_BACKSPACE

#define HU_MAXLINES 4
#define HU_MAXLINELENGTH 80


//
// Typedefs of widgets
//

// Text Doom::Line widget
//  (parent of Scrolling Text and Input Text widgets)
namespace Doom
{
struct HudTextLine
{
    // left-justified position of scrolling text window
    int x;
    int y;

    Doom::Patch** f;                        // font
    int sc;                        // start character
    char l[HU_MAXLINELENGTH + 1];        // line of text
    int len;                              // current line length

    // whether this line needs to be udpated
    int needsupdate;
};
} // namespace Doom


// Scrolling Text window widget
//  (child of Text Doom::Line widget)
namespace Doom
{
struct HudScrollingText
{
    HudTextLine l[HU_MAXLINES];        // text lines to draw
    int h;                // height in lines
    int cl;                // current line number

    // pointer to doom_boolean stating whether to update window
    doom_boolean* on;
    doom_boolean laston;                // last value of *->on.
};
} // namespace Doom


// Input Text Doom::Line widget
//  (child of Text Doom::Line widget)
namespace Doom
{
struct HudInputText
{
    HudTextLine l;                // text line to input on

     // left margin past which I am not to delete characters
    int lm;

    // pointer to doom_boolean stating whether to update window
    doom_boolean* on;
    doom_boolean laston; // last value of *->on;
};
} // namespace Doom


//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
