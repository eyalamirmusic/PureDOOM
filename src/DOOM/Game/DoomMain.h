#pragma once

#include "Event.h" // Event

// How many WADs -file may add. Was d_main.h.
#define MAXWADFILES 20

namespace Doom
{
// DOOM startup + the main game loop; d_main.cpp keeps the vanilla D_ names as
// shims. The core state d_main owns is defined at file scope in DoomMain.cpp
// (above its namespace).
void postEvent(Event* ev);
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
