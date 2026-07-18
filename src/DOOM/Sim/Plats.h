#pragma once

#include "../d_player.h" // Player (p_spec.h needs it)
#include "../p_spec.h" // plat_t, plattype_e
#include "../r_defs.h"

namespace Doom
{
// Platform thinker and handlers; p_plats.cpp keeps the vanilla names as shims.
void platRaise(plat_t& plat);
int doPlat(Line* line, plattype_e type, int amount);
void activateInStasis(int tag);
void stopPlat(Line* line);
void addActivePlat(plat_t* plat);
void removeActivePlat(plat_t* plat);
} // namespace Doom
