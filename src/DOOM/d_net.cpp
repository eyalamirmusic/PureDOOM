// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        DOOM network game communication and protocol, OS-independent parts.
//        Rewritten in Game/Net.{h,cpp}; this keeps NetUpdate / TryRunTics /
//        D_CheckNetGame / D_QuitNetGame as shims. The net buffers and tic
//        counters are defined at file scope in Net.cpp, so there is nothing to
//        own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "Game/Net.h"

void NetUpdate()
{
    Doom::netUpdate();
}

void TryRunTics()
{
    Doom::tryRunTics();
}

void D_CheckNetGame()
{
    Doom::dCheckNetGame();
}

void D_QuitNetGame()
{
    Doom::dQuitNetGame();
}
