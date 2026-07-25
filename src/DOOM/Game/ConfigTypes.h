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

#include <string_view>

//
// MISC
//
namespace Doom
{
struct ConfigDefault
{
    std::string_view name;
    int* location = nullptr;
    int defaultvalue = 0;

    // A text-valued default (defaultvalue == STRING_VALUE) writes through
    // text_location instead of location. The view does not own what it points at:
    // before the config file is read that is default_text_value's literal, and
    // after it is a slot of loadDefaults' process-lifetime string storage.
    std::string_view* text_location = nullptr;
    std::string_view default_text_value;
};

// A default whose value is text rather than a number: `defaultvalue` is this
// sentinel and the string lives in text_location.
constexpr int STRING_VALUE = 0xFFFF;

// The config default table and its length. Was two `extern` globals; storage is
// file-local to Game/Config.cpp now, reached as defaults()[i] and numdefaults().
ConfigDefault* defaults();
int numdefaults();
} // namespace Doom

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
