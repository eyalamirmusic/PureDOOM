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
//        Moving object handling. Rewritten in Sim/Mobj.{h,cpp}; this keeps the
//        vanilla names as shims. P_MobjThinker keeps its global address because
//        p_saveg and the sim probe identify mobjs by comparing to it.
//
//-----------------------------------------------------------------------------

#include "doomdef.h"
#include "p_local.h"

#include "Sim/ItemRespawnQueue.h"
#include "Sim/Mobj.h"


// The item-respawn queue. iquehead/iquetail are reset by p_setup on level load; the
// arrays are read only here and by Sim/Mobj.cpp through these definitions.
// The item respawn queue is a Doom::ItemRespawnQueue owned by the Engine now; these are
// references onto it, the arrays as references-to-array (REFACTOR.md, Step 5).
mapthing_t (&itemrespawnque)[ITEMQUESIZE] = Doom::itemRespawnQueue().itemrespawnque;
int (&itemrespawntime)[ITEMQUESIZE] = Doom::itemRespawnQueue().itemrespawntime;
int& iquehead = Doom::itemRespawnQueue().iquehead;
int& iquetail = Doom::itemRespawnQueue().iquetail;












