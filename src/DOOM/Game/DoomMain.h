#pragma once

#include "../d_event.h" // event_t

namespace Doom
{
// DOOM startup + the main game loop; d_main.cpp keeps the vanilla D_ names as
// shims. The core state d_main owns is defined at file scope in DoomMain.cpp
// (above its namespace).
void dPostEvent(event_t* ev);
void dProcessEvents(void);
void dDisplay(void);
void dUpdateWipe(void);
void dDoomLoop(void);
void dPageTicker(void);
void dPageDrawer(void);
void dAdvanceDemo(void);
void dDoAdvanceDemo(void);
void dStartTitle(void);
void dAddFile(const char* file);
void dDoomMain(void);
} // namespace Doom
