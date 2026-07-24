#pragma once

#include "../Game/PlayerTypes.h" // Player (p_spec.h needs it)
#include "SpecialTypes.h" // FloorMove, FloorType, StairType, MoveResult
#include "MapTypes.h"
#include "../Render/RenderTypes.h"

namespace Doom
{
// Floor handlers. movePlane (the shared height-mover) is now Sector::movePlane and
// the EV_ handlers are Line::doFloor / Line::buildStairs, declared on the types in
// MapTypes.h; the moving floor's per-tic behaviour is FloorMove::tick()
// (Thinkers/FloorMove.cpp). This header remains the include point that carries
// MoveResult (via SpecialTypes.h) to the floor/ceiling/plat/door thinkers.
} // namespace Doom
