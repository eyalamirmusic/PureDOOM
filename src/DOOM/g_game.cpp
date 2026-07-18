// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        The game controller (ticcmd building, the ticker/responder, level
//        load/completion, save/load, demo record/playback). Rewritten in
//        Game/Game.{h,cpp}; this keeps the G_ names as shims. The core game state
//        g_game owns is defined at file scope in Game.cpp (above its namespace),
//        so there is nothing to own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "d_ticcmd.h"
#include "g_game.h"

#include "Game/Game.h"



















