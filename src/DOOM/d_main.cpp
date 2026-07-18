// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        DOOM main program (D_DoomMain) and game loop (D_DoomLoop), plus the
//        game-mode detection, argument parsing and startup. Rewritten in
//        Game/DoomMain.{h,cpp}; this keeps the D_ names as shims. The core state
//        d_main owns is defined at file scope in DoomMain.cpp (above its
//        namespace), so there is nothing to own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "d_event.h"
#include "d_main.h"

#include "Game/DoomMain.h"












