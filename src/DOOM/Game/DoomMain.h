#pragma once

#include "../d_event.h" // event_t

namespace Doom
{
// DOOM startup + the main game loop; d_main.cpp keeps the vanilla D_ names as
// shims. The core state d_main owns is defined at file scope in DoomMain.cpp
// (above its namespace).
void dPostEvent(event_t* ev);
void dProcessEvents();
void dDisplay();
void dUpdateWipe();
void dDoomLoop();
void dPageTicker();
void dPageDrawer();
void dAdvanceDemo();
void dDoAdvanceDemo();
void dStartTitle();
void dAddFile(const char* file);
void dDoomMain();
} // namespace Doom
