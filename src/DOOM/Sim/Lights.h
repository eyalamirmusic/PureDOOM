#pragma once

#include "../Game/PlayerTypes.h" // Player (p_spec.h needs it)
#include "SpecialTypes.h" // FireFlicker, LightFlash, Strobe, Glow
#include "MapTypes.h"
#include "../Render/RenderTypes.h"

namespace Doom
{
// Light thinkers and spawners; p_lights.cpp keeps the vanilla names as shims.
void fireFlicker(FireFlicker& flick);
void spawnFireFlicker(Sector* sector);
void lightFlash(LightFlash& flash);
void spawnLightFlash(Sector* sector);
void strobeFlash(Strobe& flash);
void spawnStrobeFlash(Sector* sector, int fastOrSlow, int inSync);
void startLightStrobing(Line* line);
void turnTagLightsOff(Line* line);
void lightTurnOn(Line* line, int bright);
void glow(Glow& g);
void spawnGlowingLight(Sector* sector);
} // namespace Doom
