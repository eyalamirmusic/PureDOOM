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
//        Endianess handling, swapping 16bit and 32bit.
//
//-----------------------------------------------------------------------------

#pragma once


// Endianess handling.
// WAD files are stored little endian. The swaps live in Math/Swap.h now; this
// keeps the vanilla SHORT/LONG macros the loaders spell out, forwarding to them
// only on a big-endian host (a no-op everywhere this builds).
#ifdef __BIG_ENDIAN__
#include "Math/Swap.h"
#define SHORT(x) ((short) Doom::swap16((unsigned short) (x)))
#define LONG(x) ((long) Doom::swap32((unsigned long) (x)))
#else
#define SHORT(x) (x)
#define LONG(x) (x)
#endif



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
