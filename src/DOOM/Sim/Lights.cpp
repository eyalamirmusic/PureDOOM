// Rewritten out of vanilla p_lights into namespace Doom.
//
// Doom::Sector light effects: the fire flicker, the random and strobe flashes and the
// glow, plus their spawners and the EV_ line handlers. p_lights.cpp shims every
// name. Golden-neutral.

#include "../Host/Platform.h"

#include "../Game/GameDefs.h"
#include "Random.h"
#include "SimDefs.h"

#include "Lights.h"
#include "Tick.h" // levelAlloc / levelFree / freeLevelAllocations
#include "Specials.h"

#include <new>

#include "Random.h"
#include "../Sim/Level.h"
namespace Doom
{
// Forward declarations so the file's own call order needs no rearranging.
void spawnFireFlicker(Sector& sector);
void spawnLightFlash(Sector& sector);
void spawnStrobeFlash(Sector& sector, int fastOrSlow, int inSync);
void startLightStrobing(Line& line);
void turnTagLightsOff(Line& line);
void lightTurnOn(Line& line, int bright);
void spawnGlowingLight(Sector& sector);

//
// spawnFireFlicker
//
void spawnFireFlicker(Sector& sector)
{
    // Note that we are resetting sector attributes.
    // Nothing special about it during gameplay.
    sector.special = 0;

    FireFlicker* flick = new (levelAlloc(sizeof(*flick))) FireFlicker {};

    addThinker(*flick);

    flick->sector = &sector;
    flick->maxlight = sector.lightlevel;
    flick->minlight = findMinSurroundingLight(sector, sector.lightlevel) + 16;
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
void spawnLightFlash(Sector& sector)
{
    // nothing special about it during gameplay
    sector.special = 0;

    LightFlash* flash = new (levelAlloc(sizeof(*flash))) LightFlash {};

    addThinker(*flash);

    flash->sector = &sector;
    flash->maxlight = sector.lightlevel;

    flash->minlight = findMinSurroundingLight(sector, sector.lightlevel);
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
void spawnStrobeFlash(Sector& sector, int fastOrSlow, int inSync)
{
    Strobe* flash = new (levelAlloc(sizeof(*flash))) Strobe {};

    addThinker(*flash);

    flash->sector = &sector;
    flash->darktime = fastOrSlow;
    flash->brighttime = STROBEBRIGHT;
    flash->maxlight = sector.lightlevel;
    flash->minlight = findMinSurroundingLight(sector, sector.lightlevel);

    if (flash->minlight == flash->maxlight)
        flash->minlight = 0;

    // nothing special about it during gameplay
    sector.special = 0;

    if (!inSync)
        flash->count = (randomness().forPlay() & 7) + 1;
    else
        flash->count = 1;
}

//
// Start strobing lights (usually from a trigger)
//
void startLightStrobing(Line& line)
{
    int secnum = -1;
    while ((secnum = findSectorFromLineTag(line, secnum)) >= 0)
    {
        Sector* sec = &level().sectors[secnum];
        if (sec->specialdata)
            continue;

        spawnStrobeFlash(*sec, SLOWDARK, 0);
    }
}

//
// TURN LINE'S TAG LIGHTS OFF
//
void turnTagLightsOff(Line& line)
{
    Sector* sector = level().sectors.data();

    for (int j = 0; j < level().sectors.size(); j++, sector++)
    {
        if (sector->tag == line.tag)
        {
            int min = sector->lightlevel;
            for (int i = 0; i < sector->linecount; i++)
            {
                Line* templine = sector->lines[i];
                Sector* tsec = getNextSector(*templine, *sector);
                if (!tsec)
                    continue;
                if (tsec->lightlevel < min)
                    min = tsec->lightlevel;
            }
            sector->lightlevel = min;
        }
    }
}

//
// TURN LINE'S TAG LIGHTS ON
//
void lightTurnOn(Line& line, int bright)
{
    Sector* sector = level().sectors.data();

    for (int i = 0; i < level().sectors.size(); i++, sector++)
    {
        if (sector->tag == line.tag)
        {
            // bright = 0 means to search
            // for highest light level
            // surrounding sector
            if (!bright)
            {
                for (int j = 0; j < sector->linecount; j++)
                {
                    Line* templine = sector->lines[j];
                    Sector* temp = getNextSector(*templine, *sector);

                    if (!temp)
                        continue;

                    if (temp->lightlevel > bright)
                        bright = temp->lightlevel;
                }
            }
            sector->lightlevel = bright;
        }
    }
}

//
// Spawn glowing light
//
void spawnGlowingLight(Sector& sector)
{
    Glow* g = new (levelAlloc(sizeof(*g))) Glow {};

    addThinker(*g);

    g->sector = &sector;
    g->minlight = findMinSurroundingLight(sector, sector.lightlevel);
    g->maxlight = sector.lightlevel;
    g->direction = -1;

    sector.special = 0;
}
} // namespace Doom
