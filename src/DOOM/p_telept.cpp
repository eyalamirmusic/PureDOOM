// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Teleportation. Rewritten in Sim/Teleport.{h,cpp}; this keeps EV_Teleport.
//
//-----------------------------------------------------------------------------

#include "p_mobj.h"
#include "r_defs.h"

#include "Sim/Teleport.h"

int EV_Teleport(line_t* line, int side, mobj_t* thing)
{
    return Doom::teleport(line, side, thing);
}
