#pragma once

#include "../Game/PlayerTypes.h" // Player (p_spec.h needs it)
#include "SpecialTypes.h" // FireFlicker, LightFlash, Strobe, Glow
#include "MapTypes.h"
#include "../Render/RenderTypes.h"

namespace Doom
{
// Light spawners and handlers; p_lights.cpp keeps the vanilla names as shims. The
// per-tic behaviour is each type's tick() (Thinkers/{FireFlicker,LightFlash,Strobe,
// Glow}.cpp).
void spawnFireFlicker(Sector& sector);
void spawnLightFlash(Sector& sector);
void spawnStrobeFlash(Sector& sector, int fastOrSlow, int inSync);
void startLightStrobing(Line& line);
void turnTagLightsOff(Line& line);
void lightTurnOn(Line& line, int bright);
void spawnGlowingLight(Sector& sector);
} // namespace Doom
