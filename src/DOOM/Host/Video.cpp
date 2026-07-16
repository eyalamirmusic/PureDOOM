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

#include "../doom_config.h"

#include "../d_main.h"
#include "../doomdef.h"
#include "../doomstat.h"
#include "../i_system.h"
#include "../i_video.h"
#include "../v_video.h"

#include "Video.h"

// Read by DOOM.cpp, the menu, the status bar, the eacp port (View.h / Common.h /
// EngineAccess) and the frame hash, so it stays at file scope rather than moving
// into namespace Doom below.
unsigned char screen_palette[256 * 3];

namespace Doom
{
void I_ShutdownGraphics() {}

//
// I_StartFrame
//
void I_StartFrame() {}

void I_GetEvent() {}

//
// I_StartTic
//
void I_StartTic() {}

//
// I_UpdateNoBlit
//
void I_UpdateNoBlit()
{
    // what is this?
}

//
// I_FinishUpdate
//
void I_FinishUpdate()
{
    static int lasttic;

    // draws little dots on the bottom of the screen
    if (devparm)
    {
        int i = I_GetTime();
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
// I_ReadScreen
//
void I_ReadScreen(byte* scr)
{
    doom_memcpy(scr, screens[0], SCREENWIDTH * SCREENHEIGHT);
}

//
// I_SetPalette
//
void I_SetPalette(byte* palette)
{
    doom_memcpy(screen_palette, palette, 256 * 3);
}

void I_InitGraphics()
{
    screens[0] =
        static_cast<unsigned char*>(doom_malloc(SCREENWIDTH * SCREENHEIGHT));
}
} // namespace Doom
