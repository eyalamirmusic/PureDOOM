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
// $Log:$
//
// DESCRIPTION:
//        LineOfSight/Visibility checks, uses REJECT Lookup Table.
//
// Rewritten into Sim/Sight.{h,cpp}; this keeps the vanilla P_CheckSight name.
//
//-----------------------------------------------------------------------------

#include "doomdef.h"
#include "p_local.h"

#include "Sim/Sight.h"

doom_boolean P_CheckSight(mobj_t* t1, mobj_t* t2)
{
    return Doom::checkSight(t1, t2);
}
