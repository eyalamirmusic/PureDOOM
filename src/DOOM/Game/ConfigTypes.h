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
//
//    
//-----------------------------------------------------------------------------

#pragma once


#include "../doomtype.h"


//
// MISC
//
namespace Doom
{
struct ConfigDefault
{
    char* name;
    int* location;
    int defaultvalue;
    int scantranslate; // PC scan code hack
    int untranslated; // lousy hack
    const char**
        text_location; // [pd] int* location was used to store text pointer. Can't change to intptr_t unless we change all settings type
    char* default_text_value; // [pd] So we don't change defaultvalue behavior for int to intptr_t
};
} // namespace Doom


// A default whose value is text rather than a number: `defaultvalue` is this
// sentinel and the string lives in text_location.
#define STRING_VALUE 0xFFFF


extern Doom::ConfigDefault defaults[];
extern int numdefaults;





//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
