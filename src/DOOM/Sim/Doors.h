#pragma once

#include "../Game/PlayerTypes.h" // Player
#include "SpecialTypes.h" // Door, DoorType
#include "MapTypes.h"
#include "../Render/RenderTypes.h"

namespace Doom
{
// Door handlers, now methods on the types they key off (declared in MapTypes.h): the
// line-triggered opens are Line::doDoor / Line::doLockedDoor / Line::verticalDoor and
// the timed spawners are Sector::spawnDoorCloseIn30 / Sector::spawnDoorRaiseIn5Mins.
// The per-tic behaviour is Door::tick() (Thinkers/Door.cpp).
} // namespace Doom
