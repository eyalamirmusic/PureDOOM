#pragma once

#include "../Game/PlayerTypes.h"
#include "MobjTypes.h"
#include "MapTypes.h"
#include "../Render/RenderTypes.h"

namespace Doom
{
// Specials coordinator. The surrounding-sector height/light queries are now Sector
// methods and the line-special dispatch (crossSpecialLine/shootSpecialLine/doDonut/
// findSectorFromLineTag) are Line methods, declared in MapTypes.h. Only the
// level-wide coordinators, which have no single owner, remain free functions here.
void initPicAnims();
void updateSpecials();
void spawnSpecials();
} // namespace Doom
