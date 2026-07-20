// Rewritten out of vanilla p_plats into namespace Doom.
//
// Elevator platforms: the platRaise thinker and the EV_ handlers that spawn and
// stop them, over the global activeplats list. p_plats.cpp shims every name and owns
// the activeplats storage. Golden-neutral - the demos ride lifts.

#include "../Host/Platform.h"

#include "../Game/GameDefs.h"
#include "../Game/MapSpawns.h"
#include "Random.h"
#include "SimDefs.h"
#include "../Game/SoundData.h"

#include "Plats.h"
#include "Tick.h" // levelAlloc / levelFree / freeLevelAllocations
#include "Specials.h"

#include <new>

#include "../Game/LevelStats.h"
#include "../Game/Sound.h"
#include "../Host/System.h"
#include "ActiveSpecials.h"

#include "Floors.h"
#include "Random.h"
#include "../Sim/Level.h"
namespace Doom
{
// Forward declarations so the file's own call order needs no rearranging.
void platRaise(Plat& plat);
int doPlat(Line* line, PlatType type, int amount);
void activateInStasis(int tag);
void stopPlat(Line* line);
void addActivePlat(Plat* plat);
void removeActivePlat(Plat* plat);

void platRaise(Plat& plat)
{
    MoveResult res;

    switch (plat.status)
    {
        case up:
            res = movePlane(*plat.sector, plat.speed, plat.high, plat.crush, 0, 1);

            if (plat.type == raiseAndChange || plat.type == raiseToNearestAndChange)
            {
                if (!(levelStats().leveltime & 7))
                    startSound(reinterpret_cast<Mobj*>(&plat.sector->soundorg),
                               sfx_stnmov);
            }

            if (res == crushed && (!plat.crush))
            {
                plat.count = plat.wait;
                plat.status = down;
                startSound(reinterpret_cast<Mobj*>(&plat.sector->soundorg),
                           sfx_pstart);
            }
            else
            {
                if (res == pastdest)
                {
                    plat.count = plat.wait;
                    plat.status = waiting;
                    startSound(reinterpret_cast<Mobj*>(&plat.sector->soundorg),
                               sfx_pstop);

                    switch (plat.type)
                    {
                        case blazeDWUS:
                        case downWaitUpStay:
                            removeActivePlat(&plat);
                            break;

                        case raiseAndChange:
                        case raiseToNearestAndChange:
                            removeActivePlat(&plat);
                            break;

                        default:
                            break;
                    }
                }
            }
            break;

        case down:
            res = movePlane(*plat.sector, plat.speed, plat.low, false, 0, -1);

            if (res == pastdest)
            {
                plat.count = plat.wait;
                plat.status = waiting;
                startSound(reinterpret_cast<Mobj*>(&plat.sector->soundorg),
                           sfx_pstop);
            }
            break;

        case waiting:
            if (!--plat.count)
            {
                if (plat.sector->floorheight == plat.low)
                    plat.status = up;
                else
                    plat.status = down;
                startSound(reinterpret_cast<Mobj*>(&plat.sector->soundorg),
                           sfx_pstart);
            }

        case in_stasis:
            break;
    }
}

//
// Do Platforms
//  "amount" is only used for SOME platforms.
//
int doPlat(Line* line, PlatType type, int amount)
{
    int secnum = -1;
    int rtn = 0;

    // Activate all <type> plats that are in_stasis
    switch (type)
    {
        case perpetualRaise:
            activateInStasis(line->tag);
            break;
        default:
            break;
    }

    while ((secnum = findSectorFromLineTag(line, secnum)) >= 0)
    {
        Sector* sec = &sectors[secnum];

        if (sec->specialdata)
            continue;

        // Find lowest & highest floors around sector
        rtn = 1;
        Plat* plat = new (levelAlloc(sizeof(*plat))) Plat {};
        addThinker(plat);

        plat->type = type;
        plat->sector = sec;
        plat->sector->specialdata = plat;
        plat->crush = false;
        plat->tag = line->tag;

        switch (type)
        {
            case raiseToNearestAndChange:
                plat->speed = PLATSPEED / 2;
                sec->floorpic = sides[line->sidenum[0]].sector->floorpic;
                plat->high = findNextHighestFloor(sec, sec->floorheight);
                plat->wait = 0;
                plat->status = up;
                // NO MORE DAMAGE, IF APPLICABLE
                sec->special = 0;

                startSound(reinterpret_cast<Mobj*>(&sec->soundorg), sfx_stnmov);
                break;

            case raiseAndChange:
                plat->speed = PLATSPEED / 2;
                sec->floorpic = sides[line->sidenum[0]].sector->floorpic;
                plat->high = sec->floorheight + amount * FRACUNIT;
                plat->wait = 0;
                plat->status = up;

                startSound(reinterpret_cast<Mobj*>(&sec->soundorg), sfx_stnmov);
                break;

            case downWaitUpStay:
                plat->speed = PLATSPEED * 4;
                plat->low = findLowestFloorSurrounding(sec);

                if (plat->low > sec->floorheight)
                    plat->low = sec->floorheight;

                plat->high = sec->floorheight;
                plat->wait = 35 * PLATWAIT;
                plat->status = down;
                startSound(reinterpret_cast<Mobj*>(&sec->soundorg), sfx_pstart);
                break;

            case blazeDWUS:
                plat->speed = PLATSPEED * 8;
                plat->low = findLowestFloorSurrounding(sec);

                if (plat->low > sec->floorheight)
                    plat->low = sec->floorheight;

                plat->high = sec->floorheight;
                plat->wait = 35 * PLATWAIT;
                plat->status = down;
                startSound(reinterpret_cast<Mobj*>(&sec->soundorg), sfx_pstart);
                break;

            case perpetualRaise:
                plat->speed = PLATSPEED;
                plat->low = findLowestFloorSurrounding(sec);

                if (plat->low > sec->floorheight)
                    plat->low = sec->floorheight;

                plat->high = findHighestFloorSurrounding(sec);

                if (plat->high < sec->floorheight)
                    plat->high = sec->floorheight;

                plat->wait = 35 * PLATWAIT;
                plat->status = (PlatState) (randomness().forPlay() & 1);

                startSound(reinterpret_cast<Mobj*>(&sec->soundorg), sfx_pstart);
                break;
        }
        addActivePlat(plat);
    }

    return rtn;
}

void activateInStasis(int tag)
{
    auto& specials = activeSpecials();
    for (auto* plat: specials.activeplats)
        if (plat && plat->tag == tag && plat->status == in_stasis)
        {
            plat->status = plat->oldstatus;
            plat->stopped = false;
        }
}

void stopPlat(Line* line)
{
    auto& specials = activeSpecials();
    for (auto* plat: specials.activeplats)
        if (plat && plat->status != in_stasis && plat->tag == line->tag)
        {
            plat->oldstatus = plat->status;
            plat->status = in_stasis;
            plat->stopped = true;
        }
}

void addActivePlat(Plat* plat)
{
    auto& specials = activeSpecials();
    for (auto*& slot: specials.activeplats)
        if (slot == nullptr)
        {
            slot = plat;
            return;
        }
    fatalError("Error: addActivePlat: no more plats!");
}

void removeActivePlat(Plat* plat)
{
    auto& specials = activeSpecials();
    for (auto*& slot: specials.activeplats)
        if (plat == slot)
        {
            slot->sector->specialdata = nullptr;
            removeThinker(slot);
            slot = nullptr;

            return;
        }
    fatalError("Error: removeActivePlat: can't find plat!");
}
} // namespace Doom
