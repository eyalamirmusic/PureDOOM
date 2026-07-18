// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        DOOM system seam - timing, zone backing, startup/teardown, I_Error.
//        Rewritten in Host/System.{h,cpp}; this keeps the vanilla I_ names as
//        shims. mb_used / emptycmd are file-local to System.cpp, so there is
//        nothing to own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "i_system.h"

#include "Host/System.h"

void I_Tactile(int on, int off, int total)
{
    Doom::I_Tactile(on, off, total);
}

ticcmd_t* I_BaseTiccmd()
{
    return Doom::I_BaseTiccmd();
}

int I_GetHeapSize()
{
    return Doom::I_GetHeapSize();
}

byte* I_ZoneBase(int* size)
{
    return Doom::I_ZoneBase(size);
}

int I_GetTime()
{
    return Doom::I_GetTime();
}

void I_Init()
{
    Doom::I_Init();
}

void I_Quit()
{
    Doom::I_Quit();
}

void I_WaitVBL(int count)
{
    Doom::I_WaitVBL(count);
}

void I_BeginRead()
{
    Doom::I_BeginRead();
}

void I_EndRead()
{
    Doom::I_EndRead();
}

byte* I_AllocLow(int length)
{
    return Doom::I_AllocLow(length);
}

void I_Error(const char* error)
{
    Doom::I_Error(error);
}
