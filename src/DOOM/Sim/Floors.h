#pragma once

#include "../Game/PlayerTypes.h" // Player (p_spec.h needs it)
#include "SpecialTypes.h" // FloorMove, FloorType, StairType, MoveResult
#include "MapTypes.h"
#include "../Render/RenderTypes.h"

namespace Doom
{
// Floor thinkers and handlers; p_floor.cpp keeps the vanilla names as shims.
MoveResult movePlane(Sector& sector,
                     Fixed speed,
                     Fixed dest,
                     bool crush,
                     int floorOrCeiling,
                     int direction);
void moveFloor(FloorMove& floor);
int doFloor(Line& line, FloorType floortype);
int buildStairs(Line& line, StairType type);
} // namespace Doom
