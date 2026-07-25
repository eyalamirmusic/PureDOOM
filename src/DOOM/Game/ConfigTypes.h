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
//
//
//-----------------------------------------------------------------------------

#pragma once

#include "../doomtype.h"

#include <map>
#include <string>
#include <string_view>

//
// MISC
//
namespace Doom
{
// A default whose value is text rather than a number: `defaultvalue` is this
// sentinel and the string lives in text_value.
constexpr int STRING_VALUE = 0xFFFF;

struct ConfigDefault
{
    // Bound to the Engine member this default writes through at runtime
    // (bindEngineDefaults), not captured in the table: a static &member would take
    // the address of a reference before the Engine exists.
    int* location = nullptr;
    int defaultvalue = 0;

    // A text-valued default (defaultvalue == STRING_VALUE) writes through
    // text_location instead of location. Both strings are owned here and map nodes
    // never move, so the view text_location holds stays valid for the life of the
    // process - which it must, since the engine reads it whenever a chat macro
    // fires and saveDefaults reads it back out to write the file.
    std::string_view* text_location = nullptr;
    std::string default_text_value;
    std::string text_value;

    void setText(std::string_view value)
    {
        text_value = value;
        *text_location = text_value;
    }

    void resetToDefault()
    {
        if (defaultvalue == STRING_VALUE)
            setText(default_text_value);
        else
            *location = defaultvalue;
    }
};

// The config default table, keyed by the name that appears in ~/.doomrc. Was two
// `extern` globals, then a span over a file-local array every lookup scanned;
// storage stays file-local to Game/Config.cpp. std::less<> so a std::string_view -
// or the std::string loadDefaults reads out of the file - looks one up without
// allocating a key.
using ConfigDefaults = std::map<std::string, ConfigDefault, std::less<>>;

ConfigDefaults& defaults();
ConfigDefault* findDefault(std::string_view name);
} // namespace Doom

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
