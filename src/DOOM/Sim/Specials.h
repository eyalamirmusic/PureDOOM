#pragma once

#include "../d_player.h"
#include "../p_mobj.h"
#include "../r_defs.h"

namespace Doom
{
// Specials coordinator; p_spec.cpp keeps the vanilla names as shims.
void initPicAnims();
fixed_t findLowestFloorSurrounding(sector_t* sec);
fixed_t findHighestFloorSurrounding(sector_t* sec);
fixed_t findNextHighestFloor(sector_t* sec, int currentheight);
fixed_t findLowestCeilingSurrounding(sector_t* sec);
fixed_t findHighestCeilingSurrounding(sector_t* sec);
int findSectorFromLineTag(line_t* line, int start);
int findMinSurroundingLight(sector_t* sector, int max);
void crossSpecialLine(int linenum, int side, mobj_t* thing);
void shootSpecialLine(mobj_t* thing, line_t* line);
void playerInSpecialSector(player_t* player);
void updateSpecials();
int doDonut(line_t* line);
void spawnSpecials();
} // namespace Doom
