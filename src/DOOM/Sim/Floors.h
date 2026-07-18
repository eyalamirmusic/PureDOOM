#pragma once

#include "../d_player.h" // Player (p_spec.h needs it)
#include "../p_spec.h" // FloorMove, FloorType, StairType, MoveResult
#include "../r_defs.h"

namespace Doom
{
// Floor thinkers and handlers; p_floor.cpp keeps the vanilla names as shims.
MoveResult movePlane(Sector& sector,
                   fixed_t speed,
                   fixed_t dest,
                   doom_boolean crush,
                   int floorOrCeiling,
                   int direction);
void moveFloor(FloorMove& floor);
int doFloor(Line* line, FloorType floortype);
int buildStairs(Line* line, StairType type);
} // namespace Doom
