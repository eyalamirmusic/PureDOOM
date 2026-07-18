// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        DOOM graphics seam. Rewritten in Host/Video.{h,cpp}; this keeps the
//        vanilla I_ names as shims. screen_palette is defined at file scope in
//        Video.cpp for its many readers, so there is nothing to own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "i_system.h" // I_StartFrame / I_StartTic / I_GetEvent
#include "i_video.h"

#include "Host/Video.h"

void I_ShutdownGraphics()
{
    Doom::I_ShutdownGraphics();
}

void I_StartFrame()
{
    Doom::I_StartFrame();
}

void I_GetEvent()
{
    Doom::I_GetEvent();
}

void I_StartTic()
{
    Doom::I_StartTic();
}

void I_UpdateNoBlit()
{
    Doom::I_UpdateNoBlit();
}

void I_FinishUpdate()
{
    Doom::I_FinishUpdate();
}

void I_ReadScreen(byte* scr)
{
    Doom::I_ReadScreen(scr);
}

void I_SetPalette(byte* palette)
{
    Doom::I_SetPalette(palette);
}

void I_InitGraphics()
{
    Doom::I_InitGraphics();
}
