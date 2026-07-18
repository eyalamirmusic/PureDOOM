// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Moving ceilings/crushers. Rewritten in Sim/Ceilings.{h,cpp}; this keeps the
//        vanilla names as shims and owns the activeceilings storage (p_saveg reads
//        it). T_MoveCeiling was deleted once ThinkerDispatch.cpp started calling
//        Doom::moveCeiling directly.
//
//-----------------------------------------------------------------------------

#include "p_local.h"

#include "Sim/ActiveSpecials.h"
#include "Sim/Ceilings.h"

// activeceilings is declared in p_spec.h and read by p_saveg; it is a member of the
// Doom::ActiveSpecials owned by the Engine now, and this vanilla name a reference onto it.
ceiling_t* (&activeceilings)[MAXCEILINGS] = Doom::activeSpecials().activeceilings;

