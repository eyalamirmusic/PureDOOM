// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        The game controller (ticcmd building, the ticker/responder, level
//        load/completion, save/load, demo record/playback). Rewritten in
//        Game/Game.{h,cpp}; this keeps the G_ names as shims. The core game state
//        g_game owns is defined at file scope in Game.cpp (above its namespace),
//        so there is nothing to own here.
//
//-----------------------------------------------------------------------------

#include "doom_config.h"

#include "d_ticcmd.h"
#include "g_game.h"

#include "Game/Game.h"

void G_DeathMatchSpawnPlayer(int playernum)
{
    Doom::gDeathMatchSpawnPlayer(playernum);
}

void G_InitNew(skill_t skill, int episode, int map)
{
    Doom::gInitNew(skill, episode, map);
}

void G_DeferedInitNew(skill_t skill, int episode, int map)
{
    Doom::gDeferedInitNew(skill, episode, map);
}

void G_DeferedPlayDemo(const char* demo)
{
    Doom::gDeferedPlayDemo(demo);
}

void G_LoadGame(char* name)
{
    Doom::gLoadGame(name);
}

void G_DoLoadGame(void)
{
    Doom::gDoLoadGame();
}

void G_SaveGame(int slot, char* description)
{
    Doom::gSaveGame(slot, description);
}

void G_RecordDemo(char* name)
{
    Doom::gRecordDemo(name);
}

void G_BeginRecording(void)
{
    Doom::gBeginRecording();
}

void G_TimeDemo(char* name)
{
    Doom::gTimeDemo(name);
}

doom_boolean G_CheckDemoStatus(void)
{
    return Doom::gCheckDemoStatus();
}

void G_ExitLevel(void)
{
    Doom::gExitLevel();
}

void G_SecretExitLevel(void)
{
    Doom::gSecretExitLevel();
}

void G_WorldDone(void)
{
    Doom::gWorldDone();
}

void G_Ticker(void)
{
    Doom::gTicker();
}

doom_boolean G_Responder(event_t* ev)
{
    return Doom::gResponder(ev);
}

void G_ScreenShot(void)
{
    Doom::gScreenShot();
}

void G_BuildTiccmd(ticcmd_t* cmd)
{
    Doom::gBuildTiccmd(cmd);
}

void G_PlayerReborn(int player)
{
    Doom::gPlayerReborn(player);
}
