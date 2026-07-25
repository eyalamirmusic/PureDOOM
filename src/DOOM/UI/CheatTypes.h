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
//        Cheat code checking.
//
//-----------------------------------------------------------------------------

#pragma once

//
// CHEAT SEQUENCE PACKAGE
//

#include <span>
#include <string>

namespace Doom
{
// The target of one cheat: scrambled key codes ending in an 0xff sentinel, with
// an in-band 1 marking where typed parameters begin and zero slots waiting to
// receive them. check() writes the typed keys into those slots and param()
// reads them back out and clears them, so this is a mutable byte buffer with
// markers in it - deliberately not text, and not a string type.
struct CheatSequence
{
    std::span<unsigned char> sequence;
    int position = 0; // was the `p` cursor; 0 is the un-started state

    // The rolling match and the parameter read-back. Bodies in UI/Cheat.cpp.
    bool check(char key);
    std::string param();
};
} // namespace Doom

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
