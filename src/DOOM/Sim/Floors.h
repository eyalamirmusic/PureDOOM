#pragma once

#include "../d_player.h" // player_t (p_spec.h needs it)
#include "../p_spec.h" // floormove_t, floor_e, stair_e, result_e
#include "../r_defs.h"

namespace Doom
{
// Floor thinkers and handlers; p_floor.cpp keeps the vanilla names as shims.
result_e movePlane(sector_t& sector,
                   fixed_t speed,
                   fixed_t dest,
                   doom_boolean crush,
                   int floorOrCeiling,
                   int direction);
void moveFloor(floormove_t& floor);
int doFloor(line_t* line, floor_e floortype);
int buildStairs(line_t* line, stair_e type);
} // namespace Doom
