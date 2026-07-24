#pragma once

#include "MobjTypes.h" // Mobj
#include "MapTypes.h"
#include "../Render/RenderTypes.h" // Line

namespace Doom
{
// Teleport is Line::teleport now (declared in MapTypes.h): teleport `thing` to the
// MobjType::Teleportman in a sector tagged by this line, with fog and sound. Returns
// 1 if it teleported, 0 otherwise. Golden-neutral; the demos walk teleporters.
} // namespace Doom
