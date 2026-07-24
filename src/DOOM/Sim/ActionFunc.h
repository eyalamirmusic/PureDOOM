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

namespace Doom
{
struct Mobj;
struct Player;
struct PspDef;

// A state's action: what setMobjState / setPsprite runs when a mobj or a weapon
// enters the state. The action is a real method now, so the slot holds a
// pointer-to-member: a mobj state's action is a Mobj method (the mobj is `this`),
// a weapon state's is a Player method taking the PspDef. They never mix for a
// given state - the slot carries one of each and the caller invokes the one its
// state kind uses. Only ever one is set, and the call is
// `(mobj.*action.mobj)()` / `(player.*action.weapon)(psp)`.
//
// This is the fourth shape the slot has had. Vanilla was an "ANSI C with classes"
// union of three incompatible function-pointer types (acp1/acv/acp2) whose only
// purpose was to let one slot hold either signature. That became a single
// type-erased `void (*)(void*)` (the vanilla `actionf_p1` typedef), which each of
// Sim/Info.cpp's 449 table entries cast into and the two call sites
// reinterpret_cast back out of - well-defined, being a round trip, but invisibly
// so to both compilers, which objected on arity and needed a scoped
// -Wcast-function-type suppression to stay quiet. The third shape was two typed
// free-function pointers, void(*)(Mobj&) and void(*)(Player&, PspDef&); making
// the actions members turned those into pointers-to-member.
//
// The member pointers keep every property the third shape bought - the table
// entries (`&Mobj::chase`, `&Player::punch`) are ordinary designated initializers
// the compiler checks, there is no cast anywhere on the path, and a weapon action
// still cannot be installed on a mobj state. A pointer-to-member is wider than a
// plain function pointer, so the slot grows, but the whole table stays well under
// a few tens of KB and is still statically initialised.
//
// It stays a raw member pointer rather than a std::function, and it is the last
// one in the engine. states[] has 968 entries and is a statically initialised
// table shared by the whole engine; std::function members would make it
// dynamically initialised and non-trivially destructible, and buy nothing -
// nothing ever binds a closure to a state.
struct ActionFunc
{
    using MobjAction = void (Mobj::*)();
    using WeaponAction = void (Player::*)(PspDef&);

    MobjAction mobj = nullptr;
    WeaponAction weapon = nullptr;
};
} // namespace Doom

// The doubly-linked list node is now a real base class with a virtual tick() -
// Thinker (Sim/Thinker.h).
#include "Thinker.h"

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
