#pragma once

#include "../d_player.h" // Player
#include "../p_spec.h" // ButtonWhere
#include "../r_defs.h"

namespace Doom
{
// Switch/button handling; p_switch.cpp keeps the vanilla names as shims.
void initSwitchList();
void startButton(Line* line, ButtonWhere w, int texture, int time);
void changeSwitchTexture(Line* line, int useAgain);
doom_boolean useSpecialLine(Mobj* thing, Line* line, int side);
} // namespace Doom
