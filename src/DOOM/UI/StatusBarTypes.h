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
//        Status bar code.
//        Does the face/direction indicator animatin.
//        Does palette indicators as well (red pain/berserk, bright pickup)
//
//-----------------------------------------------------------------------------

#pragma once

#include "../doomtype.h"
#include "../Game/Event.h"

// Size of statusbar.
// Now sensitive for scaling.
#define ST_HEIGHT (32 * SCREEN_MUL)
#define ST_WIDTH SCREENWIDTH
#define ST_Y (SCREENHEIGHT - ST_HEIGHT)


//
// STATUS BAR
//

// Called by main loop.

// Called by main loop.

// Called by main loop.

// Called when the console player is spawned on each level.

// Called by startup code.


// States for status bar code.
namespace Doom
{
enum StatusBarMode
{
    AutomapState,
    FirstPersonState
};
} // namespace Doom


// States for the chat code.
namespace Doom
{
enum ChatState
{
    StartChatState,
    WaitDestState,
    GetChatState
};
} // namespace Doom




//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
