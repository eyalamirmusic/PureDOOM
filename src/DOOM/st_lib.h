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
#include "r_defs.h"


//
// Background and foreground screen numbers
//
#define STLIB_BG 4
#define STLIB_FG 0


//
// Typedefs of widgets
//

// Number widget
struct st_number_t
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

    // pointer to doom_boolean stating
    //  whether to update number
    doom_boolean* on;

    // list of patches for 0-9
    patch_t** p;

    // user data
    int data;
};


// Percent widget ("child" of number widget,
//  or, more precisely, contains a number widget.)
struct st_percent_t
{
    // number information
    st_number_t n;

    // percent sign graphic
    patch_t* p;
};


// Multiple Icon widget
struct st_multicon_t
{
    // center-justified location of icons
    int x;
    int y;

    // last icon number
    int oldinum;

    // pointer to current icon
    int* inum;

    // pointer to doom_boolean stating
    //  whether to update icon
    doom_boolean* on;

    // list of icons
    patch_t** p;

    // user data
    int data;
};


// Binary Icon widget
struct st_binicon_t
{
    // center-justified location of icon
    int x;
    int y;

    // last icon value
    int oldval;

    // pointer to current icon status
    doom_boolean* val;

    // pointer to doom_boolean
    //  stating whether to update icon
    doom_boolean* on;


    patch_t* p; // icon
    int data;   // user data

};


//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
