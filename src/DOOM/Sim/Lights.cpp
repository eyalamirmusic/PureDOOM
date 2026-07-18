// Rewritten out of vanilla p_lights into namespace Doom.
//
// Doom::Sector light effects: the fire flicker, the random and strobe flashes and the
// glow, plus their spawners and the EV_ line handlers. p_lights.cpp shims every
// name. Golden-neutral.

#include "../doom_config.h"

#include "../doomdef.h"
#include "../m_random.h"
#include "../p_local.h"
#include "../r_state.h"

#include "Lights.h"
#include "Tick.h" // levelAlloc / levelFree / freeLevelAllocations
#include "Specials.h"

#include <new>

#include "Random.h"
namespace Doom
{
// Forward declarations so the file's own call order needs no rearranging.
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

void fireFlicker(FireFlicker& flick)
{
    int amount;

    if (--flick.count)
        return;

    amount = (Doom::randomness().forPlay() & 3) * 16;

    if (flick.sector->lightlevel - amount < flick.minlight)
        flick.sector->lightlevel = flick.minlight;
    else
        flick.sector->lightlevel = flick.maxlight - amount;

    flick.count = 4;
}

//
// spawnFireFlicker
//
void spawnFireFlicker(Sector* sector)
{
    FireFlicker* flick;

    // Note that we are resetting sector attributes.
    // Nothing special about it during gameplay.
    sector->special = 0;

    flick = new (levelAlloc(sizeof(*flick))) FireFlicker {};

    Doom::addThinker(flick);

    flick->sector = sector;
    flick->maxlight = sector->lightlevel;
    flick->minlight = Doom::findMinSurroundingLight(sector, sector->lightlevel) + 16;
    flick->count = 4;
}

//
// BROKEN LIGHT FLASHING
//

//
// lightFlash
// Do flashing lights.
//
void lightFlash(LightFlash& flash)
{
    if (--flash.count)
        return;

    if (flash.sector->lightlevel == flash.maxlight)
    {
        flash.sector->lightlevel = flash.minlight;
        flash.count = (Doom::randomness().forPlay() & flash.mintime) + 1;
    }
    else
    {
        flash.sector->lightlevel = flash.maxlight;
        flash.count = (Doom::randomness().forPlay() & flash.maxtime) + 1;
    }
}

//
// spawnLightFlash
// After the map has been loaded, scan each sector
// for specials that spawn thinkers
//
void spawnLightFlash(Sector* sector)
{
    LightFlash* flash;

    // nothing special about it during gameplay
    sector->special = 0;

    flash = new (levelAlloc(sizeof(*flash))) LightFlash {};

    Doom::addThinker(flash);

    flash->sector = sector;
    flash->maxlight = sector->lightlevel;

    flash->minlight = Doom::findMinSurroundingLight(sector, sector->lightlevel);
    flash->maxtime = 64;
    flash->mintime = 7;
    flash->count = (Doom::randomness().forPlay() & flash->maxtime) + 1;
}

//
// STROBE LIGHT FLASHING
//

//
// strobeFlash
//
void strobeFlash(Strobe& flash)
{
    if (--flash.count)
        return;

    if (flash.sector->lightlevel == flash.minlight)
    {
        flash.sector->lightlevel = flash.maxlight;
        flash.count = flash.brighttime;
    }
    else
    {
        flash.sector->lightlevel = flash.minlight;
        flash.count = flash.darktime;
    }
}

//
// spawnStrobeFlash
// After the map has been loaded, scan each sector
// for specials that spawn thinkers
//
void spawnStrobeFlash(Sector* sector, int fastOrSlow, int inSync)
{
    Strobe* flash;

    flash = new (levelAlloc(sizeof(*flash))) Strobe {};

    Doom::addThinker(flash);

    flash->sector = sector;
    flash->darktime = fastOrSlow;
    flash->brighttime = STROBEBRIGHT;
    flash->maxlight = sector->lightlevel;
    flash->minlight = Doom::findMinSurroundingLight(sector, sector->lightlevel);

    if (flash->minlight == flash->maxlight)
        flash->minlight = 0;

    // nothing special about it during gameplay
    sector->special = 0;

    if (!inSync)
        flash->count = (Doom::randomness().forPlay() & 7) + 1;
    else
        flash->count = 1;
}

//
// Start strobing lights (usually from a trigger)
//
void startLightStrobing(Line* line)
{
    int secnum;
    Sector* sec;

    secnum = -1;
    while ((secnum = Doom::findSectorFromLineTag(line, secnum)) >= 0)
    {
        sec = &sectors[secnum];
        if (sec->specialdata)
            continue;

        spawnStrobeFlash(sec, SLOWDARK, 0);
    }
}

//
// TURN LINE'S TAG LIGHTS OFF
//
void turnTagLightsOff(Line* line)
{
    int min;
    Sector* sector;
    Sector* tsec;
    Line* templine;

    sector = sectors;

    for (int j = 0; j < numsectors; j++, sector++)
    {
        if (sector->tag == line->tag)
        {
            min = sector->lightlevel;
            for (int i = 0; i < sector->linecount; i++)
            {
                templine = sector->lines[i];
                tsec = getNextSector(templine, sector);
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
void lightTurnOn(Line* line, int bright)
{
    Sector* sector;
    Sector* temp;
    Line* templine;

    sector = sectors;

    for (int i = 0; i < numsectors; i++, sector++)
    {
        if (sector->tag == line->tag)
        {
            // bright = 0 means to search
            // for highest light level
            // surrounding sector
            if (!bright)
            {
                for (int j = 0; j < sector->linecount; j++)
                {
                    templine = sector->lines[j];
                    temp = getNextSector(templine, sector);

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
void glow(Glow& g)
{
    switch (g.direction)
    {
        case -1:
            // DOWN
            g.sector->lightlevel -= GLOWSPEED;
            if (g.sector->lightlevel <= g.minlight)
            {
                g.sector->lightlevel += GLOWSPEED;
                g.direction = 1;
            }
            break;

        case 1:
            // UP
            g.sector->lightlevel += GLOWSPEED;
            if (g.sector->lightlevel >= g.maxlight)
            {
                g.sector->lightlevel -= GLOWSPEED;
                g.direction = -1;
            }
            break;
    }
}

void spawnGlowingLight(Sector* sector)
{
    Glow* g;

    g = new (levelAlloc(sizeof(*g))) Glow {};

    Doom::addThinker(g);

    g->sector = sector;
    g->minlight = Doom::findMinSurroundingLight(sector, sector->lightlevel);
    g->maxlight = sector->lightlevel;
    g->direction = -1;

    sector->special = 0;
}
} // namespace Doom
