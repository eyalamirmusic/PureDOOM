#pragma once

#include "../d_player.h" // player_t
#include "../p_spec.h" // vldoor_t, vldoor_e
#include "../r_defs.h"

namespace Doom
{
// Door thinker and handlers; p_doors.cpp keeps the vanilla names as shims.
void verticalDoor(vldoor_t& door);
int doLockedDoor(Line* line, vldoor_e type, mobj_t* thing);
int doDoor(Line* line, vldoor_e type);
void verticalDoor(Line* line, mobj_t* thing);
void spawnDoorCloseIn30(Sector* sec);
void spawnDoorRaiseIn5Mins(Sector* sec, int secnum);
} // namespace Doom
