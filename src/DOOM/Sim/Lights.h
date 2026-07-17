#pragma once

#include "../d_player.h" // player_t (p_spec.h needs it)
#include "../p_spec.h" // fireflicker_t, lightflash_t, strobe_t, glow_t
#include "../r_defs.h"

namespace Doom
{
// Light thinkers and spawners; p_lights.cpp keeps the vanilla names as shims.
void fireFlicker(fireflicker_t& flick);
void spawnFireFlicker(sector_t* sector);
void lightFlash(lightflash_t& flash);
void spawnLightFlash(sector_t* sector);
void strobeFlash(strobe_t& flash);
void spawnStrobeFlash(sector_t* sector, int fastOrSlow, int inSync);
void startLightStrobing(line_t* line);
void turnTagLightsOff(line_t* line);
void lightTurnOn(line_t* line, int bright);
void glow(glow_t& g);
void spawnGlowingLight(sector_t* sector);
} // namespace Doom
