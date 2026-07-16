#pragma once

namespace Doom
{
// Savegame archive/unarchive; p_saveg.cpp keeps the vanilla names as shims.
void archivePlayers();
void unArchivePlayers();
void archiveWorld();
void unArchiveWorld();
void archiveThinkers();
void unArchiveThinkers();
void archiveSpecials();
void unArchiveSpecials();
} // namespace Doom
