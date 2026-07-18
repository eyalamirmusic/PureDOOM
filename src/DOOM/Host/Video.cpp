// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        The engine's video seam. Vanilla drove an X11 framebuffer here; in
//        PureDOOM the host (the eacp app) does the drawing, reading screens[0]
//        and the palette back out, so these are host stubs. Rewritten from
//        i_video.cpp, which keeps the vanilla I_ names as shims.
//
//-----------------------------------------------------------------------------

#include "Platform.h"
#include "../Render/Video.h"

#include "../Game/DoomMain.h"
#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"

#include "../Render/VideoState.h"
#include "Video.h"

// Read by DOOM.cpp, the menu, the status bar, the eacp port (View.h / Common.h /
#include "System.h"
// EngineAccess) and the frame hash, so it stays at file scope rather than moving
#include "../Game/LaunchOptions.h"
// into namespace Doom below.
unsigned char screen_palette[256 * 3];

namespace Doom
{
void shutdownGraphics() {}

//
// startFrame
//
void startFrame() {}

void pollHostEvent() {}

//
// startTic
//
void startTic() {}

//
// updateNoBlit
//
void updateNoBlit()
{
    // what is this?
}

//
// finishUpdate
//
void finishUpdate()
{
    static int lasttic;

    // draws little dots on the bottom of the screen
    if (launchOptions().devparm)
    {
        int i = currentTic();
        int tics = i - lasttic;
        lasttic = i;
        if (tics > 20)
            tics = 20;

        for (i = 0; i < tics * 2; i += 2)
            screens[0][(SCREENHEIGHT - 1) * SCREENWIDTH + i] = 0xff;
        for (; i < 20 * 2; i += 2)
            screens[0][(SCREENHEIGHT - 1) * SCREENWIDTH + i] = 0x0;
    }
}

//
// readScreen
//
void readScreen(byte* scr)
{
    doom_memcpy(scr, screens[0], SCREENWIDTH * SCREENHEIGHT);
}

//
// setPalette
//
void setPalette(byte* palette)
{
    doom_memcpy(screen_palette, palette, 256 * 3);
}

void initGraphics()
{
    // RAII now (Step 9): the software frame is a VideoState-owned vector; screens[0]
    // is the raw view onto its data(). This runs after Doom::initVideo, so it overwrites
    // Doom::initVideo's screens[0] slice - the framebuffer proper, which the app reads back.
    auto& frame = videoState().frame;
    frame.resize(SCREENWIDTH * SCREENHEIGHT);
    screens[0] = frame.data();
}
} // namespace Doom
