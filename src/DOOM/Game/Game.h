#pragma once

#include "Event.h" // Event
#include "Ticcmd.h" // Ticcmd
#include "GameDefs.h" // Skill

#include <string_view>

namespace Doom
{
// The game controller; g_game.cpp keeps the vanilla G_ names as shims.
void deathMatchSpawnPlayer(int playernum);
void initNewGame(Skill skill, int episode, int map);
void deferInitNew(Skill skill, int episode, int map);
void deferPlayDemo(std::string_view demo);
void loadGame(std::string_view name);
void doLoadGame();
void saveGame(int slot, std::string_view description);
void recordDemo(std::string_view name);
void beginRecording();
void startTimeDemo(std::string_view name);
bool checkDemoStatus();
void exitLevel();
void secretExitLevel();
void worldDone();
void gameTicker();
bool gameResponder(Event* ev);
void takeScreenshot();
void buildTiccmd(Ticcmd* cmd);
void playerReborn(int player);
} // namespace Doom
