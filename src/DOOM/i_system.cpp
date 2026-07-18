// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        DOOM system seam - timing, zone backing, startup/teardown, fatalError.
//        Rewritten in Host/System.{h,cpp}; this keeps the vanilla I_ names as
//        shims. mb_used / emptycmd are file-local to System.cpp, so there is
//        nothing to own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "i_system.h"

#include "Host/System.h"

void tactileFeedback(int on, int off, int total)
{
    Doom::tactileFeedback(on, off, total);
}

Doom::Ticcmd* baseTiccmd()
{
    return Doom::baseTiccmd();
}

int heapSize()
{
    return Doom::heapSize();
}

byte* zoneBase(int* size)
{
    return Doom::zoneBase(size);
}

int currentTic()
{
    return Doom::currentTic();
}

void initHost()
{
    Doom::initHost();
}

void quitGame()
{
    Doom::quitGame();
}

void waitVBlank(int count)
{
    Doom::waitVBlank(count);
}

void beginRead()
{
    Doom::beginRead();
}

void endRead()
{
    Doom::endRead();
}

byte* allocLow(int length)
{
    return Doom::allocLow(length);
}

void fatalError(const char* error)
{
    Doom::fatalError(error);
}
