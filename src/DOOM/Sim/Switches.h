#pragma once

#include "../Game/PlayerTypes.h" // Player
#include "SpecialTypes.h" // ButtonWhere
#include "MapTypes.h"
#include "../Render/RenderTypes.h"

namespace Doom
{
// Switch/button handling; p_switch.cpp keeps the vanilla names as shims.
void initSwitchList();
void startButton(Line* line, ButtonWhere w, int texture, int time);
void changeSwitchTexture(Line* line, int useAgain);
bool useSpecialLine(Mobj* thing, Line* line, int side);
} // namespace Doom
