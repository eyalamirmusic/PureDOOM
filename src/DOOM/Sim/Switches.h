#pragma once

#include "../d_player.h" // player_t
#include "../p_spec.h" // bwhere_e
#include "../r_defs.h"

namespace Doom
{
// Switch/button handling; p_switch.cpp keeps the vanilla names as shims.
void initSwitchList(void);
void startButton(line_t* line, bwhere_e w, int texture, int time);
void changeSwitchTexture(line_t* line, int useAgain);
doom_boolean useSpecialLine(mobj_t* thing, line_t* line, int side);
} // namespace Doom
