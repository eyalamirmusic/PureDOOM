#pragma once

#include "../Game/PlayerTypes.h" // Player (p_spec.h needs it)
#include "SpecialTypes.h" // Ceiling, CeilingType
#include "MapTypes.h"
#include "../Render/RenderTypes.h"

namespace Doom
{
// Ceiling handlers. The line-triggered ones are Line methods now (Line::doCeiling /
// Line::activateInStasisCeiling / Line::ceilingCrushStop, declared in MapTypes.h);
// the per-tic behaviour is Ceiling::tick() (Thinkers/Ceiling.cpp). addActiveCeiling /
// removeActiveCeiling stay free: they insert/remove a Ceiling in the level's
// activeceilings slot table, the registry role addThinker/removeThinker also keep.
void addActiveCeiling(Ceiling& c);
void removeActiveCeiling(Ceiling& c);
} // namespace Doom
