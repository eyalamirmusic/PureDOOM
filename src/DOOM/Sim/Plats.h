#pragma once

#include "../Game/PlayerTypes.h" // Player (p_spec.h needs it)
#include "SpecialTypes.h" // Plat, PlatType
#include "MapTypes.h"
#include "../Render/RenderTypes.h"

namespace Doom
{
// Platform handlers. The line-triggered ones are Line methods now (Line::doPlat /
// Line::stopPlat, declared in MapTypes.h); the per-tic behaviour is Plat::tick()
// (Thinkers/Plat.cpp). activateInStasis keys off a plain tag with no owning object,
// and addActivePlat/removeActivePlat insert/remove a Plat in the level's activeplats
// slot table (the registry role addThinker/removeThinker keep), so these stay free.
void activateInStasis(int tag);
void addActivePlat(Plat& plat);
void removeActivePlat(Plat& plat);
} // namespace Doom
