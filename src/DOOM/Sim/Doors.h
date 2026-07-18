#pragma once

#include "../d_player.h" // Player
#include "../p_spec.h" // vldoor_t, vldoor_e
#include "../r_defs.h"

namespace Doom
{
// Door thinker and handlers; p_doors.cpp keeps the vanilla names as shims.
void verticalDoor(vldoor_t& door);
int doLockedDoor(Line* line, vldoor_e type, Mobj* thing);
int doDoor(Line* line, vldoor_e type);
void verticalDoor(Line* line, Mobj* thing);
void spawnDoorCloseIn30(Sector* sec);
void spawnDoorRaiseIn5Mins(Sector* sec, int secnum);
} // namespace Doom
