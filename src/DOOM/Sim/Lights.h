#pragma once

#include "../Game/PlayerTypes.h" // Player (p_spec.h needs it)
#include "SpecialTypes.h" // FireFlicker, LightFlash, Strobe, Glow
#include "MapTypes.h"
#include "../Render/RenderTypes.h"

namespace Doom
{
// Light spawners and handlers, now methods on the types they key off (declared in
// MapTypes.h): the per-sector spawners are Sector::spawnFireFlicker /
// spawnLightFlash / spawnStrobeFlash / spawnGlowingLight and the EV_ line handlers
// are Line::startLightStrobing / turnTagLightsOff / lightTurnOn. The per-tic
// behaviour is each type's tick() (Thinkers/{FireFlicker,LightFlash,Strobe,Glow}.cpp).
} // namespace Doom
