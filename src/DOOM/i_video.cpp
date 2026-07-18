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

#include "i_system.h" // startFrame / startTic / pollHostEvent
#include "i_video.h"

#include "Host/Video.h"

void shutdownGraphics()
{
    Doom::shutdownGraphics();
}

void startFrame()
{
    Doom::startFrame();
}

void pollHostEvent()
{
    Doom::pollHostEvent();
}

void startTic()
{
    Doom::startTic();
}

void updateNoBlit()
{
    Doom::updateNoBlit();
}

void finishUpdate()
{
    Doom::finishUpdate();
}

void readScreen(byte* scr)
{
    Doom::readScreen(scr);
}

void setPalette(byte* palette)
{
    Doom::setPalette(palette);
}

void initGraphics()
{
    Doom::initGraphics();
}
