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
// DESCRIPTION:
//         The status bar widget code.
//
//-----------------------------------------------------------------------------

#pragma once

// We are referring to patches.
#include "../Sim/MapTypes.h"
#include "../Render/RenderTypes.h"

namespace Doom
{
//
// Background and foreground screen numbers
//
constexpr int STLIB_BG = 4;
constexpr int STLIB_FG = 0;

//
// Typedefs of widgets
//

// Number widget
struct StatusNumber
{
    // upper right-hand corner
    //  of the number (right-justified)
    int x = 0;
    int y = 0;

    // max # of digits in number
    int width = 0;

    // last number value
    int oldnum = 0;

    // pointer to current value
    int* num = nullptr;

    // pointer to bool stating
    //  whether to update number
    bool* on = nullptr;

    // list of patches for 0-9
    Patch** p = nullptr;

    // user data
    int data = 0;
};

// Percent widget ("child" of number widget,
//  or, more precisely, contains a number widget.)
struct StatusPercent
{
    // number information
    StatusNumber n;

    // percent sign graphic
    Patch* p = nullptr;
};

// Multiple Icon widget
struct StatusMultIcon
{
    // center-justified location of icons
    int x = 0;
    int y = 0;

    // last icon number
    int oldinum = 0;

    // pointer to current icon
    int* inum = nullptr;

    // pointer to bool stating
    //  whether to update icon
    bool* on = nullptr;

    // list of icons
    Patch** p = nullptr;

    // user data
    int data = 0;
};

// Binary Icon widget
struct StatusBinIcon
{
    // center-justified location of icon
    int x = 0;
    int y = 0;

    // last icon value
    int oldval = 0;

    // pointer to current icon status
    bool* val = nullptr;

    // pointer to bool
    //  stating whether to update icon
    bool* on = nullptr;

    Patch* p = nullptr; // icon
    int data = 0; // user data
};
} // namespace Doom

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
