// Rewritten out of vanilla p_lights into namespace Doom.
//
// Sector light effects: the fire flicker, the random and strobe flashes and the
// glow, plus their spawners and the EV_ line handlers. The T_ thinker functions
// stay global (p_saveg identifies a thinker by comparing to them, and the spawners
// store their global address); p_lights.cpp shims every name. Golden-neutral.

#include "../doom_config.h"

#include "../doomdef.h"
#include "../m_random.h"
#include "../p_local.h"
#include "../r_state.h"

#include "Lights.h"
#include "Tick.h" // levelAlloc / levelFree / freeLevelAllocations

// The thinker functions stay global (p_saveg identity); declared so the spawners
// can store their address.
void T_FireFlicker(fireflicker_t* flick);
void T_LightFlash(lightflash_t* flash);
void T_StrobeFlash(strobe_t* flash);
void T_Glow(glow_t* g);

namespace Doom
{
// Forward declarations so the file's own call order needs no rearranging.
void fireFlicker(fireflicker_t* flick);
void spawnFireFlicker(sector_t* sector);
void lightFlash(lightflash_t* flash);
void spawnLightFlash(sector_t* sector);
void strobeFlash(strobe_t* flash);
void spawnStrobeFlash(sector_t* sector, int fastOrSlow, int inSync);
void startLightStrobing(line_t* line);
void turnTagLightsOff(line_t* line);
void lightTurnOn(line_t* line, int bright);
void glow(glow_t* g);
void spawnGlowingLight(sector_t* sector);

void fireFlicker(fireflicker_t* flick)
{
    int amount;

    if (--flick->count)
        return;

    amount = (P_Random() & 3) * 16;

    if (flick->sector->lightlevel - amount < flick->minlight)
        flick->sector->lightlevel = flick->minlight;
    else
        flick->sector->lightlevel = flick->maxlight - amount;

    flick->count = 4;
}

//
// spawnFireFlicker
//
void spawnFireFlicker(sector_t* sector)
{
    fireflicker_t* flick;

    // Note that we are resetting sector attributes.
    // Nothing special about it during gameplay.
    sector->special = 0;

    flick = (fireflicker_t*) (levelAlloc(sizeof(*flick)));

    P_AddThinker(&flick->thinker);

    flick->thinker.function.acp1 = (actionf_p1) T_FireFlicker;
    flick->sector = sector;
    flick->maxlight = sector->lightlevel;
    flick->minlight = P_FindMinSurroundingLight(sector, sector->lightlevel) + 16;
    flick->count = 4;
}

//
// BROKEN LIGHT FLASHING
//

//
// lightFlash
// Do flashing lights.
//
void lightFlash(lightflash_t* flash)
{
    if (--flash->count)
        return;

    if (flash->sector->lightlevel == flash->maxlight)
    {
        flash->sector->lightlevel = flash->minlight;
        flash->count = (P_Random() & flash->mintime) + 1;
    }
    else
    {
        flash->sector->lightlevel = flash->maxlight;
        flash->count = (P_Random() & flash->maxtime) + 1;
    }
}

//
// spawnLightFlash
// After the map has been loaded, scan each sector
// for specials that spawn thinkers
//
void spawnLightFlash(sector_t* sector)
{
    lightflash_t* flash;

    // nothing special about it during gameplay
    sector->special = 0;

    flash = (lightflash_t*) (levelAlloc(sizeof(*flash)));

    P_AddThinker(&flash->thinker);

    flash->thinker.function.acp1 = (actionf_p1) T_LightFlash;
    flash->sector = sector;
    flash->maxlight = sector->lightlevel;

    flash->minlight = P_FindMinSurroundingLight(sector, sector->lightlevel);
    flash->maxtime = 64;
    flash->mintime = 7;
    flash->count = (P_Random() & flash->maxtime) + 1;
}

//
// STROBE LIGHT FLASHING
//

//
// strobeFlash
//
void strobeFlash(strobe_t* flash)
{
    if (--flash->count)
        return;

    if (flash->sector->lightlevel == flash->minlight)
    {
        flash->sector->lightlevel = flash->maxlight;
        flash->count = flash->brighttime;
    }
    else
    {
        flash->sector->lightlevel = flash->minlight;
        flash->count = flash->darktime;
    }
}

//
// spawnStrobeFlash
// After the map has been loaded, scan each sector
// for specials that spawn thinkers
//
void spawnStrobeFlash(sector_t* sector, int fastOrSlow, int inSync)
{
    strobe_t* flash;

    flash = (strobe_t*) (levelAlloc(sizeof(*flash)));

    P_AddThinker(&flash->thinker);

    flash->sector = sector;
    flash->darktime = fastOrSlow;
    flash->brighttime = STROBEBRIGHT;
    flash->thinker.function.acp1 = (actionf_p1) T_StrobeFlash;
    flash->maxlight = sector->lightlevel;
    flash->minlight = P_FindMinSurroundingLight(sector, sector->lightlevel);

    if (flash->minlight == flash->maxlight)
        flash->minlight = 0;

    // nothing special about it during gameplay
    sector->special = 0;

    if (!inSync)
        flash->count = (P_Random() & 7) + 1;
    else
        flash->count = 1;
}

//
// Start strobing lights (usually from a trigger)
//
void startLightStrobing(line_t* line)
{
    int secnum;
    sector_t* sec;

    secnum = -1;
    while ((secnum = P_FindSectorFromLineTag(line, secnum)) >= 0)
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
void turnTagLightsOff(line_t* line)
{
    int i;
    int j;
    int min;
    sector_t* sector;
    sector_t* tsec;
    line_t* templine;

    sector = sectors;

    for (j = 0; j < numsectors; j++, sector++)
    {
        if (sector->tag == line->tag)
        {
            min = sector->lightlevel;
            for (i = 0; i < sector->linecount; i++)
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
void lightTurnOn(line_t* line, int bright)
{
    int i;
    int j;
    sector_t* sector;
    sector_t* temp;
    line_t* templine;

    sector = sectors;

    for (i = 0; i < numsectors; i++, sector++)
    {
        if (sector->tag == line->tag)
        {
            // bright = 0 means to search
            // for highest light level
            // surrounding sector
            if (!bright)
            {
                for (j = 0; j < sector->linecount; j++)
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
void glow(glow_t* g)
{
    switch (g->direction)
    {
        case -1:
            // DOWN
            g->sector->lightlevel -= GLOWSPEED;
            if (g->sector->lightlevel <= g->minlight)
            {
                g->sector->lightlevel += GLOWSPEED;
                g->direction = 1;
            }
            break;

        case 1:
            // UP
            g->sector->lightlevel += GLOWSPEED;
            if (g->sector->lightlevel >= g->maxlight)
            {
                g->sector->lightlevel -= GLOWSPEED;
                g->direction = -1;
            }
            break;
    }
}

void spawnGlowingLight(sector_t* sector)
{
    glow_t* g;

    g = (glow_t*) (levelAlloc(sizeof(*g)));

    P_AddThinker(&g->thinker);

    g->sector = sector;
    g->minlight = P_FindMinSurroundingLight(sector, sector->lightlevel);
    g->maxlight = sector->lightlevel;
    g->thinker.function.acp1 = (actionf_p1) T_Glow;
    g->direction = -1;

    sector->special = 0;
}
} // namespace Doom
