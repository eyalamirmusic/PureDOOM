#pragma once

#include "../d_player.h" // Player (p_spec.h needs it)
#include "../p_spec.h" // Ceiling, CeilingType
#include "../r_defs.h"

namespace Doom
{
// Ceiling thinker and handlers; p_ceilng.cpp keeps the vanilla names as shims.
void moveCeiling(Ceiling& ceiling);
int doCeiling(Line* line, CeilingType type);
void addActiveCeiling(Ceiling* c);
void removeActiveCeiling(Ceiling* c);
void activateInStasisCeiling(Line* line);
int ceilingCrushStop(Line* line);
} // namespace Doom
