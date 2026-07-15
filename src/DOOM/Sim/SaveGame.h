#pragma once

namespace Doom
{
// Savegame archive/unarchive; p_saveg.cpp keeps the vanilla names as shims.
void archivePlayers(void);
void unArchivePlayers(void);
void archiveWorld(void);
void unArchiveWorld(void);
void archiveThinkers(void);
void unArchiveThinkers(void);
void archiveSpecials(void);
void unArchiveSpecials(void);
} // namespace Doom
