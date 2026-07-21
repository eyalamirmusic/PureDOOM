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
namespace Doom
{
constexpr int ST_HEIGHT = 32 * SCREEN_MUL;
constexpr int ST_WIDTH = SCREENWIDTH;
constexpr int ST_Y = SCREENHEIGHT - ST_HEIGHT;
} // namespace Doom
