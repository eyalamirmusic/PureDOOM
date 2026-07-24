#pragma once

#include "../Game/PlayerTypes.h"
#include "MobjTypes.h"
#include "MapTypes.h"
#include "../Render/RenderTypes.h"

namespace Doom
{
// Specials coordinator; p_spec.cpp keeps the vanilla names as shims.
void initPicAnims();
Fixed findLowestFloorSurrounding(Sector& sec);
Fixed findHighestFloorSurrounding(Sector& sec);
Fixed findNextHighestFloor(Sector& sec, Fixed currentheight);
Fixed findLowestCeilingSurrounding(Sector& sec);
Fixed findHighestCeilingSurrounding(Sector& sec);
int findSectorFromLineTag(Line& line, int start);
int findMinSurroundingLight(Sector& sector, int max);
void crossSpecialLine(int linenum, int side, Mobj& thing);
void shootSpecialLine(Mobj& thing, Line& line);
void playerInSpecialSector(Player& player);
void updateSpecials();
int doDonut(Line& line);
void spawnSpecials();
} // namespace Doom
