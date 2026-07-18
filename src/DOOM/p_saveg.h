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
//        Savegame I/O, archiving, persistence.
//
//-----------------------------------------------------------------------------

#pragma once


#include "doomtype.h"


// Persistent storage/archiving.
// These are the load / save game routines.

// a reference onto Doom::SaveGameState's cursor (an Engine member) - the storage moved
// off this loose global in Step 5
extern byte*& save_p;



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
