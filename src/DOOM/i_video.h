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


#include "doomtype.h"


// Called by Doom::doomMain,
// determines the hardware configuration
// and sets up the video mode


// Takes full 8 bit values.


// Wait for vertical retrace or pause a bit.
void I_WaitVBL(int count);


void I_BeginRead();
void I_EndRead();



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
