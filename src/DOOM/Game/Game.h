#pragma once

#include "../d_event.h" // Event
#include "../d_ticcmd.h" // Ticcmd
#include "../doomdef.h" // Skill

namespace Doom
{
// The game controller; g_game.cpp keeps the vanilla G_ names as shims.
void deathMatchSpawnPlayer(int playernum);
void initNewGame(Skill skill, int episode, int map);
void deferInitNew(Skill skill, int episode, int map);
void deferPlayDemo(const char* demo);
void loadGame(char* name);
void doLoadGame();
void saveGame(int slot, char* description);
void recordDemo(char* name);
void beginRecording();
void startTimeDemo(char* name);
doom_boolean checkDemoStatus();
void exitLevel();
void secretExitLevel();
void worldDone();
void gameTicker();
doom_boolean gameResponder(Event* ev);
void takeScreenshot();
void buildTiccmd(Ticcmd* cmd);
void playerReborn(int player);
} // namespace Doom
