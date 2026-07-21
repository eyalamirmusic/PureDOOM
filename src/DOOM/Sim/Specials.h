#pragma once

#include "../Game/PlayerTypes.h"
#include "MobjTypes.h"
#include "MapTypes.h"
#include "../Render/RenderTypes.h"

namespace Doom
{
// Specials coordinator; p_spec.cpp keeps the vanilla names as shims.
void initPicAnims();
fixed_t findLowestFloorSurrounding(Sector& sec);
fixed_t findHighestFloorSurrounding(Sector& sec);
fixed_t findNextHighestFloor(Sector& sec, fixed_t currentheight);
fixed_t findLowestCeilingSurrounding(Sector& sec);
fixed_t findHighestCeilingSurrounding(Sector& sec);
int findSectorFromLineTag(Line& line, int start);
int findMinSurroundingLight(Sector& sector, int max);
void crossSpecialLine(int linenum, int side, Mobj& thing);
void shootSpecialLine(Mobj& thing, Line& line);
void playerInSpecialSector(Player& player);
void updateSpecials();
int doDonut(Line& line);
void spawnSpecials();
} // namespace Doom
