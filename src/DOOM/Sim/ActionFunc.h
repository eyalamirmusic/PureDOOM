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
//  MapObj data. Map Objects or mobjs are actors, entities,
//  thinker, take-your-pick... anything that moves, acts, or
//  suffers state changes of more or less violent nature.
//
//-----------------------------------------------------------------------------

#pragma once


//
// A state's action: the free function setMobjState / setPsprite runs when a mobj
// or weapon enters the state. mobj states are called with (Doom::Mobj*), weapon states
// with (Doom::Player*, Doom::PspDef*) - the two shapes never mix for a given state. The
// pointer is stored type-erased (fn) and cast back to its exact signature at the
// two call sites, which is a round-trip conversion and therefore well-defined.
//
// This retired the 1993 "ANSI C with classes" union of three incompatible
// function-pointer types (acp1/acv/acp2), whose only purpose was to let one slot
// hold either shape. The generated states[] table (Sim/Info.cpp) still casts each
// action to actionf_p1 as it stores it, so that type is kept.
//
typedef void (*actionf_p1)(void*);

namespace Doom
{
struct ActionFunc
{
    actionf_p1 fn;
};
} // namespace Doom


// The doubly-linked list node is now a real base class with a virtual tick() -
// Doom::Thinker (Sim/Thinker.h). Doom::Thinker stays as the vanilla spelling the
// engine still writes, aliased onto it.
#include "Thinker.h"



//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
