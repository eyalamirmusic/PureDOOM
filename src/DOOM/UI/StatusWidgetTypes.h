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

//
// Background and foreground screen numbers
//
#define STLIB_BG 4
#define STLIB_FG 0

//
// Typedefs of widgets
//

// Number widget
namespace Doom
{
struct StatusNumber
{
    // upper right-hand corner
    //  of the number (right-justified)
    int x;
    int y;

    // max # of digits in number
    int width;

    // last number value
    int oldnum;

    // pointer to current value
    int* num;

    // pointer to bool stating
    //  whether to update number
    bool* on;

    // list of patches for 0-9
    Doom::Patch** p;

    // user data
    int data;
};
} // namespace Doom

// Percent widget ("child" of number widget,
//  or, more precisely, contains a number widget.)
namespace Doom
{
struct StatusPercent
{
    // number information
    StatusNumber n;

    // percent sign graphic
    Doom::Patch* p;
};
} // namespace Doom

// Multiple Icon widget
namespace Doom
{
struct StatusMultIcon
{
    // center-justified location of icons
    int x;
    int y;

    // last icon number
    int oldinum;

    // pointer to current icon
    int* inum;

    // pointer to bool stating
    //  whether to update icon
    bool* on;

    // list of icons
    Doom::Patch** p;

    // user data
    int data;
};
} // namespace Doom

// Binary Icon widget
namespace Doom
{
struct StatusBinIcon
{
    // center-justified location of icon
    int x;
    int y;

    // last icon value
    int oldval;

    // pointer to current icon status
    bool* val;

    // pointer to bool
    //  stating whether to update icon
    bool* on;

    Doom::Patch* p; // icon
    int data; // user data
};
} // namespace Doom

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
