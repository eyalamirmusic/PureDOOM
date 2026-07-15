// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Default config file, PCX screenshots, raw file I/O, M_DrawText.
//        Rewritten in Game/Config.{h,cpp}; this keeps the M_ names as shims. The
//        defaults[] table is defined at file scope in Config.cpp, so there is
//        nothing to own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "m_misc.h"

#include "Game/Config.h"

int M_DrawText(int x, int y, doom_boolean direct, char* string)
{
    return Doom::mDrawText(x, y, direct, string);
}

doom_boolean M_WriteFile(char const* name, void* source, int length)
{
    return Doom::mWriteFile(name, source, length);
}

int M_ReadFile(char const* name, byte** buffer)
{
    return Doom::mReadFile(name, buffer);
}

void M_SaveDefaults(void)
{
    Doom::mSaveDefaults();
}

void M_LoadDefaults(void)
{
    Doom::mLoadDefaults();
}

void M_ScreenShot(void)
{
    Doom::mScreenShot();
}
