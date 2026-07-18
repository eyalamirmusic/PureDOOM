#pragma once

#include "../d_player.h" // Player (p_spec.h needs it)
#include "../p_spec.h" // fireflicker_t, lightflash_t, strobe_t, glow_t
#include "../r_defs.h"

namespace Doom
{
// Light thinkers and spawners; p_lights.cpp keeps the vanilla names as shims.
void fireFlicker(fireflicker_t& flick);
void spawnFireFlicker(Sector* sector);
void lightFlash(lightflash_t& flash);
void spawnLightFlash(Sector* sector);
void strobeFlash(strobe_t& flash);
void spawnStrobeFlash(Sector* sector, int fastOrSlow, int inSync);
void startLightStrobing(Line* line);
void turnTagLightsOff(Line* line);
void lightTurnOn(Line* line, int bright);
void glow(glow_t& g);
void spawnGlowingLight(Sector* sector);
} // namespace Doom
