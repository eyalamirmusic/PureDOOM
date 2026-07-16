#pragma once

#include "../d_event.h" // event_t
#include "../d_ticcmd.h" // ticcmd_t
#include "../doomdef.h" // skill_t

namespace Doom
{
// The game controller; g_game.cpp keeps the vanilla G_ names as shims.
void gDeathMatchSpawnPlayer(int playernum);
void gInitNew(skill_t skill, int episode, int map);
void gDeferedInitNew(skill_t skill, int episode, int map);
void gDeferedPlayDemo(const char* demo);
void gLoadGame(char* name);
void gDoLoadGame();
void gSaveGame(int slot, char* description);
void gRecordDemo(char* name);
void gBeginRecording();
void gTimeDemo(char* name);
doom_boolean gCheckDemoStatus();
void gExitLevel();
void gSecretExitLevel();
void gWorldDone();
void gTicker();
doom_boolean gResponder(event_t* ev);
void gScreenShot();
void gBuildTiccmd(ticcmd_t* cmd);
void gPlayerReborn(int player);
} // namespace Doom
