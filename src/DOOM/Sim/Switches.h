#pragma once

#include "../Game/PlayerTypes.h" // Player
#include "SpecialTypes.h" // ButtonWhere
#include "MapTypes.h"
#include "../Render/RenderTypes.h"

namespace Doom
{
// Switch/button handling. startButton / changeSwitchTexture / useSpecialLine are Line
// methods now (declared in MapTypes.h); initSwitchList builds the level's switch-pair
// table and has no owning object, so it stays a free function.
void initSwitchList();
} // namespace Doom
