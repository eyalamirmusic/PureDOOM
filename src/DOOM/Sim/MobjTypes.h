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
//        Map Objects, MObj, definition and handling.
//
//-----------------------------------------------------------------------------

#pragma once

// The Mobj thinker (struct Mobj and MobjFlag) moved to Thinkers/Mobj.h when every
// thinker got its own tick() there; this stays the header the rest of the engine
// includes for `Mobj, MapThing, StateNum, MobjType`, and re-exports it (along with
// everything it pulls in - Info, MapFormat, the fixed-point/trig tables).
#include "../Thinkers/Mobj.h"
