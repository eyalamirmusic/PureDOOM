#pragma once

#include "../d_player.h" // Player (p_spec.h needs it)
#include "../p_spec.h" // ceiling_t, ceiling_e
#include "../r_defs.h"

namespace Doom
{
// Ceiling thinker and handlers; p_ceilng.cpp keeps the vanilla names as shims.
void moveCeiling(ceiling_t& ceiling);
int doCeiling(Line* line, ceiling_e type);
void addActiveCeiling(ceiling_t* c);
void removeActiveCeiling(ceiling_t* c);
void activateInStasisCeiling(Line* line);
int ceilingCrushStop(Line* line);
} // namespace Doom
