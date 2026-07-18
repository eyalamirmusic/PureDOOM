#pragma once

#include "../d_event.h" // event_t

namespace Doom
{
// DOOM startup + the main game loop; d_main.cpp keeps the vanilla D_ names as
// shims. The core state d_main owns is defined at file scope in DoomMain.cpp
// (above its namespace).
void postEvent(event_t* ev);
void processEvents();
void displayFrame();
void updateWipe();
void doomLoop();
void pageTicker();
void drawPage();
void advanceDemo();
void doAdvanceDemo();
void startTitle();
void addWadFile(const char* file);
void doomMain();
} // namespace Doom
