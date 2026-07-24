// Rewritten out of vanilla p_ceilng into namespace Doom.
//
// Moving ceilings and crushers: the moveCeiling thinker and the EV_ handlers over
// the global activeceilings list. p_ceilng.cpp shims every name and owns the
// activeceilings storage. Golden-neutral - the demos trip crushers.

#include "../Host/Platform.h"

#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"
#include "SimDefs.h"
#include "../Game/SoundData.h"

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
int doCeiling(Line& line, CeilingType type);
void addActiveCeiling(Ceiling& c);
void removeActiveCeiling(Ceiling& c);
void activateInStasisCeiling(Line& line);
int ceilingCrushStop(Line& line);

//
// doCeiling
// Move a ceiling up/down and all around!
//
int doCeiling(Line& line, CeilingType type)
{
    int secnum = -1;
    int rtn = 0;

    // Reactivate in-stasis ceilings...for certain types.
    switch (type)
    {
        case CeilingType::FastCrushAndRaise:
        case CeilingType::SilentCrushAndRaise:
        case CeilingType::CrushAndRaise:
            activateInStasisCeiling(line);
            break;
        case CeilingType::LowerToFloor:
        case CeilingType::RaiseToHighest:
        case CeilingType::LowerAndCrush:
            break;
    }

    while ((secnum = findSectorFromLineTag(line, secnum)) >= 0)
    {
        Sector* sec = &level().sectors[secnum];
        if (sec->specialdata)
            continue;

        // new door thinker
        rtn = 1;
        Ceiling* ceiling = new (levelAlloc(sizeof(*ceiling))) Ceiling {};
        addThinker(*ceiling);
        sec->specialdata = ceiling;
        ceiling->sector = sec;
        ceiling->crush = false;

        switch (type)
        {
            case CeilingType::FastCrushAndRaise:
                ceiling->crush = true;
                ceiling->topheight = sec->ceilingheight;
                ceiling->bottomheight = sec->floorheight + (8 * FRACUNIT);
                ceiling->direction = -1;
                ceiling->speed = CEILSPEED * 2;
                break;

            case CeilingType::SilentCrushAndRaise:
            case CeilingType::CrushAndRaise:
                ceiling->crush = true;
                ceiling->topheight = sec->ceilingheight;
                [[fallthrough]];
            case CeilingType::LowerAndCrush:
            case CeilingType::LowerToFloor:
                ceiling->bottomheight = sec->floorheight;
                if (type != CeilingType::LowerToFloor)
                    ceiling->bottomheight += 8 * FRACUNIT;
                ceiling->direction = -1;
                ceiling->speed = CEILSPEED;
                break;

            case CeilingType::RaiseToHighest:
                ceiling->topheight = findHighestCeilingSurrounding(*sec);
                ceiling->direction = 1;
                ceiling->speed = CEILSPEED;
                break;
        }

        ceiling->tag = sec->tag;
        ceiling->type = type;
        addActiveCeiling(*ceiling);
    }

    return rtn;
}

//
// Add an active ceiling
//
void addActiveCeiling(Ceiling& c)
{
    auto& specials = activeSpecials();
    for (auto*& ceiling: specials.activeceilings)
    {
        if (ceiling == nullptr)
        {
            ceiling = &c;
            return;
        }
    }
}

//
// Remove a ceiling's thinker
//
void removeActiveCeiling(Ceiling& c)
{
    auto& specials = activeSpecials();
    for (auto*& ceiling: specials.activeceilings)
    {
        if (ceiling == &c)
        {
            ceiling->sector->specialdata = nullptr;
            removeThinker(*ceiling);
            ceiling = nullptr;
            break;
        }
    }
}

//
// Restart a ceiling that's in-stasis
//
void activateInStasisCeiling(Line& line)
{
    auto& specials = activeSpecials();
    for (auto* ceiling: specials.activeceilings)
    {
        if (ceiling && ceiling->tag == line.tag && ceiling->direction == 0)
        {
            ceiling->direction = ceiling->olddirection;
            ceiling->stopped = false;
        }
    }
}

//
// ceilingCrushStop
// Stop a ceiling from crushing!
//
int ceilingCrushStop(Line& line)
{
    int rtn = 0;
    auto& specials = activeSpecials();
    for (auto* ceiling: specials.activeceilings)
    {
        if (ceiling && ceiling->tag == line.tag && ceiling->direction != 0)
        {
            ceiling->olddirection = ceiling->direction;
            ceiling->stopped = true;
            ceiling->direction = 0; // in-stasis
            rtn = 1;
        }
    }

    return rtn;
}
} // namespace Doom
