// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Command-line arguments. Rewritten in Game/Args.{h,cpp}; this keeps
//        M_CheckParm as a shim. myargc/myargv are defined at file scope in
//        Args.cpp, so there is nothing to own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "Game/Args.h"

int M_CheckParm(const char* check)
{
    return Doom::checkParm(check);
}
