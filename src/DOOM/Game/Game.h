#pragma once

#include "../d_event.h" // event_t
#include "../d_ticcmd.h" // ticcmd_t
#include "../doomdef.h" // skill_t

namespace Doom
{
// The game controller; g_game.cpp keeps the vanilla G_ names as shims.
void deathMatchSpawnPlayer(int playernum);
void initNewGame(skill_t skill, int episode, int map);
void deferInitNew(skill_t skill, int episode, int map);
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
doom_boolean gameResponder(event_t* ev);
void takeScreenshot();
void buildTiccmd(ticcmd_t* cmd);
void playerReborn(int player);
} // namespace Doom
