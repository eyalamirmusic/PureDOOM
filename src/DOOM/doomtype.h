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

// `doom_boolean` used to live here - an int, deliberately not the built-in bool,
// because compiled as C it was `enum { false, true }` and the engine leaned on its
// width wherever a boolean was read through a pointer to something else. It is gone:
// every one of its ~288 uses is a real `bool` now (REFACTOR.md, Step 9).
//
// Three declarations had to stop lying before the flip could happen, and all three
// are still `int` on purpose, each with the reason recorded at its own site - do not
// "tidy" them into bool:
//
//   - Render/Data.cpp's `MapTexture::masked`, overlaid on raw TEXTURE1/TEXTURE2 lump
//     bytes, where its four bytes hold the following fields in place.
//   - Game/GameSession.h's `deathmatch`, which is tri-state (0 coop, 1 deathmatch,
//     2 altdeath) and would silently collapse 1 and 2.
//   - Sim/Specials.cpp's `AnimDef::istexture`, whose animdefs table terminates on a
//     `-1` sentinel that would read as `true` and never end the scan.
//
// And one pun had to be untangled: `ST_createWidgets` bound the ARMS widget with
// vanilla's own `(int*) &plyr->weaponowned[i + 1]` cast and read four bytes back out
// through it, which against a one-byte bool is three bytes of the neighbouring struct
// - and for i == 5, of the `int ammo[]` past the end of the array. The widget indexes
// `StatusBarWidgets::w_armsindex[6]` instead now.

typedef unsigned char byte;

#include <type_traits>

namespace Doom
{
// The integer an enum value names. Several scoped enums here index a parallel table -
// states[], mobjinfo[], S_sfx[], Player::ammo[], Player::cards[] - and this is how such
// a conversion is spelled, rather than a bare static_cast at each site.
//
// Deliberately explicit, and deliberately a function rather than an implicit conversion:
// `enum class` exists precisely to stop enum-to-int happening silently, so the point is
// to make each index conversion visible and greppable, not to hide it again. It returns
// `int` because that is what eacp's containers are indexed by.
template <typename E>
    requires std::is_enum_v<E>
constexpr int toIndex(E value)
{
    return static_cast<int>(value);
}

// Bit-flag helpers, for the scoped flag enums (MobjFlag, CheatFlag, ButtonCode).
//
// The flag *words* stay plain ints: Mobj and MobjInfo are memcpy'd whole by the
// savegame, Ticcmd::buttons goes onto the wire, and the mobjinfo[] table composes
// flag sets at compile time. So the enum types the individual flags and these spell
// the combination, rather than operator overloads making enum-to-int implicit again
// - which is the thing `enum class` is here to prevent.
//
// hasFlag is an ANY-of test, matching what `bits & MASK` meant at every call site
// here: true when at least one named flag is set.
template <typename E>
    requires std::is_enum_v<E>
constexpr int flagBits(E flag)
{
    return toIndex(flag);
}

template <typename E, typename... Rest>
    requires std::is_enum_v<E>
constexpr int flagBits(E first, Rest... rest)
{
    return toIndex(first) | flagBits(rest...);
}

template <typename... E>
constexpr bool hasFlag(int bits, E... flags)
{
    return (bits & flagBits(flags...)) != 0;
}

template <typename... E>
constexpr int withFlags(int bits, E... flags)
{
    return bits | flagBits(flags...);
}

template <typename... E>
constexpr int withoutFlags(int bits, E... flags)
{
    return bits & ~flagBits(flags...);
}

template <typename... E>
constexpr int toggledFlags(int bits, E... flags)
{
    return bits ^ flagBits(flags...);
}
} // namespace Doom

#define DOOM_MAXSHORT ((short) 0x7fff)

// Max pos 32-bit int.
#define DOOM_MAXINT ((int) 0x7fffffff)

// Max negative 32-bit integer.
#define DOOM_MININT ((int) 0x80000000)

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
