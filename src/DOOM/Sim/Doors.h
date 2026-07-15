#pragma once

#include "../d_player.h" // player_t
#include "../p_spec.h" // vldoor_t, vldoor_e
#include "../r_defs.h"

namespace Doom
{
// Door thinker and handlers; p_doors.cpp keeps the vanilla names as shims.
void verticalDoor(vldoor_t* door);
int doLockedDoor(line_t* line, vldoor_e type, mobj_t* thing);
int doDoor(line_t* line, vldoor_e type);
void verticalDoor(line_t* line, mobj_t* thing);
void spawnDoorCloseIn30(sector_t* sec);
void spawnDoorRaiseIn5Mins(sector_t* sec, int secnum);
} // namespace Doom
