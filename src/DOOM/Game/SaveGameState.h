#pragma once

#include "../doomtype.h"

namespace Doom
{
// The savegame serialization / orchestration state. G_DoSaveGame / G_DoLoadGame set
// name (the file), read it into buffer, then archive or unarchive the world by walking
// cursor along buffer: the P_Archive* / P_UnArchive* functions (Sim/SaveGame) advance
// cursor a field at a time, and the written length is cursor - buffer.
//
//   cursor = save_p       buffer = savebuffer       name = savename
//
// Three loose globals gathered here by Step 5's file-scope-statics sweep, now that the
// p_saveg net (Tests/Sim/SaveGameTests.cpp, doomSimSaveLoadPreservesWorld) pins the
// serialization path: save_p was the p_saveg.cpp shim's global (externed in p_saveg.h,
// read by g_game, the probe and Sim/SaveGame - both externs move to a reference in
// lockstep, or a bare one clobbers the reference's pointer), buffer and name g_game's
// own file-scope. The vanilla names become references onto these members. cursor is live
// test-covered (the probe sets it and walks it through P_ArchiveThinkers /
// P_UnArchiveThinkers); buffer and name are exercised by the app's save/load, and a
// reference-alias is pure storage relocation regardless.
struct SaveGameState
{
    byte* cursor = nullptr; // save_p: the read/write cursor
    byte* buffer = nullptr; // savebuffer: the block cursor walks
    char name[256] = {};    // savename: the file being saved/loaded
};

// The one SaveGameState, a view onto the Engine's member - the same pattern as the
// other clusters.
SaveGameState& saveGameState();
} // namespace Doom
