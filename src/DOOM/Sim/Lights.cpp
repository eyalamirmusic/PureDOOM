// Rewritten out of vanilla p_lights into namespace Doom.
//
// Sector light effects: the fire flicker, the random and strobe flashes and the
// glow, plus their spawners and the EV_ line handlers. p_lights.cpp shims every
// name. Golden-neutral.

#include "../Host/Platform.h"

#include "../Game/GameDefs.h"
#include "Random.h"
#include "SimDefs.h"

#include "Lights.h"
#include "Tick.h" // addThinker / removeThinker
#include "Specials.h"

#include "Random.h"
#include "../Sim/Level.h"
namespace Doom
{
// The per-sector light spawners are Sector methods and the EV_ line handlers are
// Line methods now (declared on the types in MapTypes.h); no forward declarations
// needed.

//
// spawnFireFlicker
//
void Sector::spawnFireFlicker()
{
    // Note that we are resetting sector attributes.
    // Nothing special about it during gameplay.
    special = 0;

    auto* flick = &addThinker<FireFlicker>();

    flick->sector = this;
    flick->maxlight = lightlevel;
    flick->minlight = findMinSurroundingLight(lightlevel) + 16;
    flick->count = 4;
}

//
// BROKEN LIGHT FLASHING
//

//
// spawnLightFlash
// After the map has been loaded, scan each sector
// for specials that spawn thinkers
//
void Sector::spawnLightFlash()
{
    // nothing special about it during gameplay
    special = 0;

    auto* flash = &addThinker<LightFlash>();

    flash->sector = this;
    flash->maxlight = lightlevel;

    flash->minlight = findMinSurroundingLight(lightlevel);
    flash->maxtime = 64;
    flash->mintime = 7;
    flash->count = (randomness().forPlay() & flash->maxtime) + 1;
}

//
// STROBE LIGHT FLASHING
//

//
// spawnStrobeFlash
// After the map has been loaded, scan each sector
// for specials that spawn thinkers
//
void Sector::spawnStrobeFlash(int fastOrSlow, int inSync)
{
    auto* flash = &addThinker<Strobe>();

    flash->sector = this;
    flash->darktime = fastOrSlow;
    flash->brighttime = STROBEBRIGHT;
    flash->maxlight = lightlevel;
    flash->minlight = findMinSurroundingLight(lightlevel);

    if (flash->minlight == flash->maxlight)
        flash->minlight = 0;

    // nothing special about it during gameplay
    special = 0;

    if (!inSync)
        flash->count = (randomness().forPlay() & 7) + 1;
    else
        flash->count = 1;
}

//
// Start strobing lights (usually from a trigger)
//
void Line::startLightStrobing()
{
    auto secnum = -1;
    while ((secnum = findSectorFromLineTag(secnum)) >= 0)
    {
        auto* sec = &level().sectors[secnum];
        if (sec->specialdata)
            continue;

        sec->spawnStrobeFlash(SLOWDARK, 0);
    }
}

//
// TURN LINE'S TAG LIGHTS OFF
//
void Line::turnTagLightsOff()
{
    for (auto& sector: level().sectors)
    {
        if (sector.tag == tag)
        {
            auto min = sector.lightlevel;
            for (auto* templine: sector.lineSlice())
            {
                auto* tsec = templine->nextSector(sector);
                if (!tsec)
                    continue;
                if (tsec->lightlevel < min)
                    min = tsec->lightlevel;
            }
            sector.lightlevel = min;
        }
    }
}

//
// TURN LINE'S TAG LIGHTS ON
//
void Line::lightTurnOn(int bright)
{
    for (auto& sector: level().sectors)
    {
        if (sector.tag == tag)
        {
            // bright = 0 means to search
            // for highest light level
            // surrounding sector
            if (!bright)
            {
                for (auto* templine: sector.lineSlice())
                {
                    auto* temp = templine->nextSector(sector);

                    if (!temp)
                        continue;

                    if (temp->lightlevel > bright)
                        bright = temp->lightlevel;
                }
            }
            sector.lightlevel = bright;
        }
    }
}

//
// Spawn glowing light
//
void Sector::spawnGlowingLight()
{
    auto* g = &addThinker<Glow>();

    g->sector = this;
    g->minlight = findMinSurroundingLight(lightlevel);
    g->maxlight = lightlevel;
    g->direction = -1;

    special = 0;
}
} // namespace Doom
