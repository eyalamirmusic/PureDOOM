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
//  Intermission.
//
//-----------------------------------------------------------------------------

#pragma once

#include "doomdef.h"

// States for the intermission
namespace Doom
{
enum IntermissionPhase
{
    NoState = -1,
    StatCount,
    ShowNextLoc
};
} // namespace Doom

// Called by main loop, animate the intermission.

// Called by main loop,
// draws the intermission directly into the screen buffer.

// Setup for an intermission screen.


//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
