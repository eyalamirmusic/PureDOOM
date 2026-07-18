#pragma once

#include "../d_player.h" // player_t
#include "../p_spec.h" // bwhere_e
#include "../r_defs.h"

namespace Doom
{
// Switch/button handling; p_switch.cpp keeps the vanilla names as shims.
void initSwitchList();
void startButton(Line* line, bwhere_e w, int texture, int time);
void changeSwitchTexture(Line* line, int useAgain);
doom_boolean useSpecialLine(mobj_t* thing, Line* line, int side);
} // namespace Doom
