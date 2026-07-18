#pragma once

#include "../d_player.h" // Player
#include "../p_spec.h" // Door, DoorType
#include "../r_defs.h"

namespace Doom
{
// Door thinker and handlers; p_doors.cpp keeps the vanilla names as shims.
void verticalDoor(Door& door);
int doLockedDoor(Line* line, DoorType type, Mobj* thing);
int doDoor(Line* line, DoorType type);
void verticalDoor(Line* line, Mobj* thing);
void spawnDoorCloseIn30(Sector* sec);
void spawnDoorRaiseIn5Mins(Sector* sec, int secnum);
} // namespace Doom
