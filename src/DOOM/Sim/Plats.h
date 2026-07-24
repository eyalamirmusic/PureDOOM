#pragma once

#include "../Game/PlayerTypes.h" // Player (p_spec.h needs it)
#include "SpecialTypes.h" // Plat, PlatType
#include "MapTypes.h"
#include "../Render/RenderTypes.h"

namespace Doom
{
// Platform handlers; p_plats.cpp keeps the vanilla names as shims. The per-tic
// behaviour is Plat::tick() (Thinkers/Plat.cpp).
int doPlat(Line& line, PlatType type, int amount);
void activateInStasis(int tag);
void stopPlat(Line& line);
void addActivePlat(Plat& plat);
void removeActivePlat(Plat& plat);
} // namespace Doom
