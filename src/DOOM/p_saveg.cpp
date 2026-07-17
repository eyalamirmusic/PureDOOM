// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification only under the
// terms of the DOOM Source Code License. See the license for details.
//
// DESCRIPTION:
//        Savegame I/O. Rewritten in Sim/SaveGame.{h,cpp}; this keeps the vanilla
//        names as shims and owns the save_p cursor.
//
//-----------------------------------------------------------------------------

#include "p_local.h"

#include "Game/SaveGameState.h"
#include "Sim/SaveGame.h"

// The savegame read/write cursor, advanced by g_game around these calls. Its storage
// lives in Doom::SaveGameState (an Engine member) now; this is a reference onto it, so
// the engine-wide readers (g_game, Sim/SaveGame, the probe) resolve unchanged.
byte*& save_p = Doom::saveGameState().cursor;

void P_ArchivePlayers(void)
{
    Doom::archivePlayers();
}

void P_UnArchivePlayers(void)
{
    Doom::unArchivePlayers();
}

void P_ArchiveWorld(void)
{
    Doom::archiveWorld();
}

void P_UnArchiveWorld(void)
{
    Doom::unArchiveWorld();
}

void P_ArchiveThinkers(void)
{
    Doom::archiveThinkers();
}

void P_UnArchiveThinkers(void)
{
    Doom::unArchiveThinkers();
}

void P_ArchiveSpecials(void)
{
    Doom::archiveSpecials();
}

void P_UnArchiveSpecials(void)
{
    Doom::unArchiveSpecials();
}
