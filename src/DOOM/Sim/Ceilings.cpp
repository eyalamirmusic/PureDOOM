// Rewritten out of vanilla p_ceilng into namespace Doom.
//
// Moving ceilings and crushers: the moveCeiling thinker and the EV_ handlers over
// the global activeceilings list. p_ceilng.cpp shims every name and owns the
// activeceilings storage. Golden-neutral - the demos trip crushers.

#include "../doom_config.h"

#include "../doomdef.h"
#include "../doomstat.h"
#include "../p_local.h"
#include "../sounds.h"

#include "Ceilings.h"
#include "Tick.h" // levelAlloc / levelFree / freeLevelAllocations
#include "Specials.h"

#include <new>

#include "../Game/LevelStats.h"
#include "../Game/Sound.h"

#include "ActiveSpecials.h"
#include "Floors.h"
#include "../Sim/Level.h"
namespace Doom
{
// Forward declarations so the file's own call order needs no rearranging.
void moveCeiling(Ceiling& ceiling);
int doCeiling(Line* line, CeilingType type);
void addActiveCeiling(Ceiling* c);
void removeActiveCeiling(Ceiling* c);
void activateInStasisCeiling(Line* line);
int ceilingCrushStop(Line* line);

void moveCeiling(Ceiling& ceiling)
{
    MoveResult res;

    auto& stats = levelStats();

    switch (ceiling.direction)
    {
        case 0:
            // IN STASIS
            break;
        case 1:
            // UP
            res = Doom::movePlane(*ceiling.sector,
                              ceiling.speed,
                              ceiling.topheight,
                              false,
                              1,
                              ceiling.direction);

            if (!(stats.leveltime & 7))
            {
                switch (ceiling.type)
                {
                    case silentCrushAndRaise:
                        break;
                    default:
                        Doom::startSound(
                            reinterpret_cast<Mobj*>(&ceiling.sector->soundorg),
                            sfx_stnmov);
                        // ?
                        break;
                }
            }

            if (res == pastdest)
            {
                switch (ceiling.type)
                {
                    case raiseToHighest:
                        removeActiveCeiling(&ceiling);
                        break;

                    case silentCrushAndRaise:
                        Doom::startSound(
                            reinterpret_cast<Mobj*>(&ceiling.sector->soundorg),
                            sfx_pstop);
                    case fastCrushAndRaise:
                    case crushAndRaise:
                        ceiling.direction = -1;
                        break;

                    default:
                        break;
                }
            }
            break;

        case -1:
            // DOWN
            res = Doom::movePlane(*ceiling.sector,
                              ceiling.speed,
                              ceiling.bottomheight,
                              ceiling.crush,
                              1,
                              ceiling.direction);

            if (!(stats.leveltime & 7))
            {
                switch (ceiling.type)
                {
                    case silentCrushAndRaise:
                        break;
                    default:
                        Doom::startSound(
                            reinterpret_cast<Mobj*>(&ceiling.sector->soundorg),
                            sfx_stnmov);
                }
            }

            if (res == pastdest)
            {
                switch (ceiling.type)
                {
                    case silentCrushAndRaise:
                        Doom::startSound(
                            reinterpret_cast<Mobj*>(&ceiling.sector->soundorg),
                            sfx_pstop);
                    case crushAndRaise:
                        ceiling.speed = CEILSPEED;
                    case fastCrushAndRaise:
                        ceiling.direction = 1;
                        break;

                    case lowerAndCrush:
                    case lowerToFloor:
                        removeActiveCeiling(&ceiling);
                        break;

                    default:
                        break;
                }
            }
            else // ( res != pastdest )
            {
                if (res == crushed)
                {
                    switch (ceiling.type)
                    {
                        case silentCrushAndRaise:
                        case crushAndRaise:
                        case lowerAndCrush:
                            ceiling.speed = CEILSPEED / 8;
                            break;

                        default:
                            break;
                    }
                }
            }
            break;
    }
}

//
// doCeiling
// Move a ceiling up/down and all around!
//
int doCeiling(Line* line, CeilingType type)
{
    int secnum;
    int rtn;
    Sector* sec;
    Ceiling* ceiling;

    secnum = -1;
    rtn = 0;

    // Reactivate in-stasis ceilings...for certain types.
    switch (type)
    {
        case fastCrushAndRaise:
        case silentCrushAndRaise:
        case crushAndRaise:
            activateInStasisCeiling(line);
        default:
            break;
    }

    while ((secnum = Doom::findSectorFromLineTag(line, secnum)) >= 0)
    {
        sec = &sectors[secnum];
        if (sec->specialdata)
            continue;

        // new door thinker
        rtn = 1;
        ceiling = new (levelAlloc(sizeof(*ceiling))) Ceiling {};
        Doom::addThinker(ceiling);
        sec->specialdata = ceiling;
        ceiling->sector = sec;
        ceiling->crush = false;

        switch (type)
        {
            case fastCrushAndRaise:
                ceiling->crush = true;
                ceiling->topheight = sec->ceilingheight;
                ceiling->bottomheight = sec->floorheight + (8 * FRACUNIT);
                ceiling->direction = -1;
                ceiling->speed = CEILSPEED * 2;
                break;

            case silentCrushAndRaise:
            case crushAndRaise:
                ceiling->crush = true;
                ceiling->topheight = sec->ceilingheight;
            case lowerAndCrush:
            case lowerToFloor:
                ceiling->bottomheight = sec->floorheight;
                if (type != lowerToFloor)
                    ceiling->bottomheight += 8 * FRACUNIT;
                ceiling->direction = -1;
                ceiling->speed = CEILSPEED;
                break;

            case raiseToHighest:
                ceiling->topheight = Doom::findHighestCeilingSurrounding(sec);
                ceiling->direction = 1;
                ceiling->speed = CEILSPEED;
                break;
        }

        ceiling->tag = sec->tag;
        ceiling->type = type;
        addActiveCeiling(ceiling);
    }

    return rtn;
}

//
// Add an active ceiling
//
void addActiveCeiling(Ceiling* c)
{
    auto& specials = activeSpecials();
    for (int i = 0; i < MAXCEILINGS; i++)
    {
        if (specials.activeceilings[i] == nullptr)
        {
            specials.activeceilings[i] = c;
            return;
        }
    }
}

//
// Remove a ceiling's thinker
//
void removeActiveCeiling(Ceiling* c)
{
    auto& specials = activeSpecials();
    for (int i = 0; i < MAXCEILINGS; i++)
    {
        if (specials.activeceilings[i] == c)
        {
            specials.activeceilings[i]->sector->specialdata = nullptr;
            Doom::removeThinker(specials.activeceilings[i]);
            specials.activeceilings[i] = nullptr;
            break;
        }
    }
}

//
// Restart a ceiling that's in-stasis
//
void activateInStasisCeiling(Line* line)
{
    auto& specials = activeSpecials();
    for (int i = 0; i < MAXCEILINGS; i++)
    {
        if (specials.activeceilings[i]
            && (specials.activeceilings[i]->tag == line->tag)
            && (specials.activeceilings[i]->direction == 0))
        {
            specials.activeceilings[i]->direction =
                specials.activeceilings[i]->olddirection;
            specials.activeceilings[i]->stopped = false;
        }
    }
}

//
// ceilingCrushStop
// Stop a ceiling from crushing!
//
int ceilingCrushStop(Line* line)
{
    int rtn;

    rtn = 0;
    auto& specials = activeSpecials();
    for (int i = 0; i < MAXCEILINGS; i++)
    {
        if (specials.activeceilings[i]
            && (specials.activeceilings[i]->tag == line->tag)
            && (specials.activeceilings[i]->direction != 0))
        {
            specials.activeceilings[i]->olddirection =
                specials.activeceilings[i]->direction;
            specials.activeceilings[i]->stopped = true;
            specials.activeceilings[i]->direction = 0; // in-stasis
            rtn = 1;
        }
    }

    return rtn;
}
} // namespace Doom
