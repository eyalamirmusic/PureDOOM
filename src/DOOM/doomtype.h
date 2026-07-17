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
//        Simple basic typedefs, isolated here to make it easier
//         separating modules.
//    
//-----------------------------------------------------------------------------

#pragma once


// An int, and NOT the built-in bool, even though the engine is C++ now.
//
// Compiled as C this was `enum { false, true }`, which is int-sized, and the
// engine leans on that in places where a boolean is read through a pointer to
// something else. `ST_createWidgets` binds the ARMS widget with
//
//     STlib_initMultIcon(..., (int*) &plyr->weaponowned[i + 1], ...)
//
// - vanilla's own cast - and `STlib_updateMultIcon` then reads four bytes back
// out through that `int*`. Against a one-byte bool those reads are three bytes of
// neighbouring struct, the icon index comes out as garbage, and the status bar
// draws a null patch on the first tic of the first demo.
//
// Making it a real bool is a change to the engine's behaviour, not to its
// spelling, so it belongs to the refactor proper (REFACTOR.md, Step 5 onward),
// one subsystem at a time with the demos watching - not to the language flip,
// whose whole promise is that nothing moves.
typedef int doom_boolean;


typedef unsigned char byte;


#define DOOM_MAXCHAR    ((char)0x7f)
#define DOOM_MAXSHORT   ((short)0x7fff)

// Max pos 32-bit int.
#define DOOM_MAXINT     ((int)0x7fffffff)        
#define DOOM_MAXLONG    ((long)0x7fffffff)
#define DOOM_MINCHAR    ((char)0x80)
#define DOOM_MINSHORT   ((short)0x8000)

// Max negative 32-bit integer.
#define DOOM_MININT     ((int)0x80000000)        
#define DOOM_MINLONG    ((long)0x80000000)



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
