#pragma once

#include "../d_player.h" // player_t (p_spec.h needs it)
#include "../p_spec.h" // plat_t, plattype_e
#include "../r_defs.h"

namespace Doom
{
// Platform thinker and handlers; p_plats.cpp keeps the vanilla names as shims.
void platRaise(plat_t& plat);
int doPlat(line_t* line, plattype_e type, int amount);
void activateInStasis(int tag);
void stopPlat(line_t* line);
void addActivePlat(plat_t* plat);
void removeActivePlat(plat_t* plat);
} // namespace Doom
