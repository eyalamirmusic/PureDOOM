#pragma once

#include "../doomdef.h" // skill_t

namespace Doom
{
// Level setup; p_setup.cpp keeps the vanilla names as shims.
void loadVertexes(int lump);
void loadSegs(int lump);
void loadSubsectors(int lump);
void loadSectors(int lump);
void loadNodes(int lump);
void loadThings(int lump);
void loadLineDefs(int lump);
void loadSideDefs(int lump);
void loadBlockMap(int lump);
void groupLines(void);
void setupLevel(int episode, int map, int playermask, skill_t skill);
void init(void);
} // namespace Doom
