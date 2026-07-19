#pragma once

#include "../doomtype.h"

#include <ea_data_structures/Structures/Array.h>
#include <ea_data_structures/Structures/Vector.h>

namespace Doom
{
// The savegame serialization / orchestration state. G_DoSaveGame / Doom::doLoadGame set
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
// test-covered (the probe sets it and walks it through Doom::archiveThinkers /
// Doom::unArchiveThinkers); buffer and name are exercised by the app's save/load, and a
// reference-alias is pure storage relocation regardless.
//
// buffer is used two ways (REFACTOR.md, Step 9 strand (b)), so it cannot become an owning
// vector itself: doLoadGame() points it at the bytes read off disk, while doSaveGame()
// instead points it at screens[1] + 0x4000 - a view into the software framebuffer's scratch
// area, which must never be freed. loadStorage is therefore the owner for the load path
// alone, filled directly by Doom::readFile (whose out-parameter became this owner, its only
// caller being right here), and buffer stays a raw VIEW pointer either way: at loadStorage
// on the load path, at screens[1] on the save path, freed through neither. That also retires
// the manual doom_free 38 lines further down, which an early `return` on a bad savegame
// version used to skip.
struct SaveGameState
{
    byte* cursor = nullptr; // save_p: the read/write cursor
    EA::Vector<byte>
        loadStorage; // owns doLoadGame()'s bytes; buffer views it, see above
    byte* buffer = nullptr; // savebuffer: the block cursor walks
    EA::Array<char, 256> name = {}; // savename: the file being saved/loaded
};

// The one SaveGameState, a view onto the Engine's member - the same pattern as the
// other clusters.
SaveGameState& saveGameState();
} // namespace Doom
