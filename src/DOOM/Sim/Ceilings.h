#pragma once

#include "../d_player.h" // player_t (p_spec.h needs it)
#include "../p_spec.h" // ceiling_t, ceiling_e
#include "../r_defs.h"

namespace Doom
{
// Ceiling thinker and handlers; p_ceilng.cpp keeps the vanilla names as shims.
void moveCeiling(ceiling_t* ceiling);
int doCeiling(line_t* line, ceiling_e type);
void addActiveCeiling(ceiling_t* c);
void removeActiveCeiling(ceiling_t* c);
void activateInStasisCeiling(line_t* line);
int ceilingCrushStop(line_t* line);
} // namespace Doom
