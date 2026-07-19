// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        The engine's system seam - timing, startup/teardown and the fatal
//        path - over the doom_config host hooks (doom_gettime, doom_print,
//        doom_exit). Rewritten from i_system.cpp.
//
//-----------------------------------------------------------------------------

#include "Platform.h"

#include "../Game/NetTypes.h"
#include "../Game/GameDefs.h"
#include "../Game/ConfigTypes.h"

#include "System.h"

// In Doom::DemoState (an Engine member); read by fatalError to flush a demo before it aborts.
#include "../Game/Config.h"
#include "../Game/Net.h"
#include "Sound.h"
#include "Video.h"
#include "../Game/Game.h"

#include "../Game/DemoState.h"
namespace Doom
{
Ticcmd emptycmd;

void tactileFeedback(int, int, int) {}

Ticcmd* baseTiccmd()
{
    return &emptycmd;
}

//
// currentTic
// returns time in 1/70th second tics
//
int currentTic()
{
    int sec, usec;
    static int basetime = 0;

    doom_gettime(&sec, &usec);
    if (!basetime)
        basetime = sec;
    int newtics = (sec - basetime) * TICRATE + usec * TICRATE / 1000000;
    return newtics;
}

//
// initHost
//
void initHost()
{
    initSoundHost();
}

//
// quitGame
//
void quitGame()
{
    Doom::quitNetGame();
    shutdownSoundHost();
    shutdownMusic();
    Doom::saveDefaults();
    shutdownGraphics();
    doom_exit(0);
}

void waitVBlank(int)
{
#if 0 // [pd] Never sleep in main thread
#ifdef SGI
    sginap(1);
#else
#ifdef SUN
    sleep(0);
#else
    usleep(count * (1000000 / 70));
#endif
#endif
#endif
}

void beginRead() {}

void endRead() {}

byte* allocLow(int length)
{
    byte* mem = static_cast<byte*>(doom_malloc(length));
    doom_memset(mem, 0, length);
    return mem;
}

//
// fatalError
//
void fatalError(const char* error)
{
    // Message first.
    if (error)
        doom_print(error);
    doom_print("\n");

    // Shutdown. Here might be other errors.
    if (demoState().demorecording)
        Doom::checkDemoStatus();

    Doom::quitNetGame();
    shutdownGraphics();

    doom_exit(-1);
}
} // namespace Doom
