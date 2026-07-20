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

#include <ea_data_structures/Structures/Array.h>

#include <string>

// background and foreground screen numbers
// different from other modules.
namespace Doom
{
constexpr int FG = 0;

constexpr int HU_MAXLINES = 4;
constexpr int HU_MAXLINELENGTH = 80;
} // namespace Doom

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

    Patch** f; // font
    int sc; // start character
    std::string l; // line of text

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
    EA::Array<HudTextLine, HU_MAXLINES> l; // text lines to draw
    int h; // height in lines
    int cl; // current line number

    // pointer to bool stating whether to update window
    bool* on;
    bool laston; // last value of *->on.
};
} // namespace Doom

// Input Text Doom::Line widget
//  (child of Text Doom::Line widget)
namespace Doom
{
struct HudInputText
{
    HudTextLine l; // text line to input on

    // left margin past which I am not to delete characters
    int lm;

    // pointer to bool stating whether to update window
    bool* on;
    bool laston; // last value of *->on;
};
} // namespace Doom

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
