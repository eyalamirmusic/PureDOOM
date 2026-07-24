#pragma once

#include "../Game/PlayerTypes.h" // Player
#include "SpecialTypes.h" // Door, DoorType
#include "MapTypes.h"
#include "../Render/RenderTypes.h"

namespace Doom
{
// Door handlers; p_doors.cpp keeps the vanilla names as shims. The per-tic
// behaviour is Door::tick() (Thinkers/Door.cpp); verticalDoor(Line&, Mobj&) below
// is the separate line-triggered open.
int doLockedDoor(Line& line, DoorType type, Mobj& thing);
int doDoor(Line& line, DoorType type);
void verticalDoor(Line& line, Mobj& thing);
void spawnDoorCloseIn30(Sector& sec);
void spawnDoorRaiseIn5Mins(Sector& sec, int secnum);
} // namespace Doom
