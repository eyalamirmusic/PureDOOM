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
//
// $Log:$
//
// DESCRIPTION:
//        DOOM strings, by language.
//
//-----------------------------------------------------------------------------

#pragma once


// All important printed strings.
// Doom::Language selection (message strings).
// Use -DFRENCH etc.

#ifdef FRENCH
#include "StringsFrench.h"
#else
#include "StringsEnglish.h"
#endif


// Misc. other strings.
namespace Doom
{
constexpr const char* SAVEGAMENAME = "doomsav";
} // namespace Doom

//
// File locations,
//  relative to current position.
// Path names are OS-sensitive.
//
// DEVMAPS/DEVDATA stay macros deliberately: Game/DoomMain.cpp's -shdev/-regdev/
// -comdev paths and its -wart handling build wad/config paths with adjacent-literal
// concatenation ("~" DEVMAPS "E", DEVDATA "doom1.wad", ...), which happens at
// translation phase 6 and which a constexpr const char* cannot do. Same reason as
// StringsEnglish.h's PRESSKEY/PRESSYN and DOSY.
#define DEVMAPS "devmaps"
#define DEVDATA "devdata"

// Not done in french?

// QuitDOOM messages
namespace Doom
{
constexpr int NUM_QUITMESSAGES = 22;
} // namespace Doom


extern const char* endmsg[];



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
