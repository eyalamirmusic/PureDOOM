// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Switches, buttons. Rewritten in Sim/Switches.{h,cpp}; this keeps the
//        vanilla names as shims and owns the buttonlist storage (p_spec ticks it).
//
//-----------------------------------------------------------------------------

#include "p_local.h"

#include "Sim/ActiveSpecials.h"
#include "Sim/Switches.h"

// buttonlist is declared in p_spec.h and ticked by p_spec's Doom::updateSpecials; it is a member
// of the Doom::ActiveSpecials owned by the Engine now, and this vanilla name a reference onto it.
button_t (&buttonlist)[MAXBUTTONS] = Doom::activeSpecials().buttonlist;




