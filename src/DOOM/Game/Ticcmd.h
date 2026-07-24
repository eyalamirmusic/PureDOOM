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
//        System specific interface stuff.
//
//-----------------------------------------------------------------------------

#pragma once

#include "../doomtype.h"

// The data sampled per tick (single player)
// and transmitted to other peers (multiplayer).
// Mainly movements/button commands per game tick,
// plus a checksum for internal state consistency.
namespace Doom
{
struct Ticcmd
{
    char forwardmove = 0; // *2048 for move
    char sidemove = 0; // *2048 for move
    short angleturn = 0; // <<16 for angle delta
    short consistancy = 0; // checks for net game
    byte chatchar = 0;
    byte buttons = 0;
};
} // namespace Doom

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
