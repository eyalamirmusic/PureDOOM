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
int doPlat(Line& line, PlatType type, int amount);
void activateInStasis(int tag);
void stopPlat(Line& line);
void addActivePlat(Plat& plat);
void removeActivePlat(Plat& plat);

void platRaise(Plat& plat)
{
    MoveResult res;

    switch (plat.status)
    {
        case PlatState::Up:
            res = movePlane(*plat.sector, plat.speed, plat.high, plat.crush, 0, 1);

            if (plat.type == PlatType::RaiseAndChange
                || plat.type == PlatType::RaiseToNearestAndChange)
            {
                if (!(levelStats().leveltime & 7))
                    startSound(reinterpret_cast<Mobj*>(&plat.sector->soundorg),
                               SfxEnum::Stnmov);
            }

            if (res == MoveResult::Crushed && (!plat.crush))
            {
                plat.count = plat.wait;
                plat.status = PlatState::Down;
                startSound(reinterpret_cast<Mobj*>(&plat.sector->soundorg),
                           SfxEnum::Pstart);
            }
            else
            {
                if (res == MoveResult::PastDest)
                {
                    plat.count = plat.wait;
                    plat.status = PlatState::Waiting;
                    startSound(reinterpret_cast<Mobj*>(&plat.sector->soundorg),
                               SfxEnum::Pstop);

                    switch (plat.type)
                    {
                        case PlatType::BlazeDWUS:
                        case PlatType::DownWaitUpStay:
                            removeActivePlat(plat);
                            break;

                        case PlatType::RaiseAndChange:
                        case PlatType::RaiseToNearestAndChange:
                            removeActivePlat(plat);
                            break;

                        case PlatType::PerpetualRaise:
                            break;
                    }
                }
            }
            break;

        case PlatState::Down:
            res = movePlane(*plat.sector, plat.speed, plat.low, false, 0, -1);

            if (res == MoveResult::PastDest)
            {
                plat.count = plat.wait;
                plat.status = PlatState::Waiting;
                startSound(reinterpret_cast<Mobj*>(&plat.sector->soundorg),
                           SfxEnum::Pstop);
            }
            break;

        case PlatState::Waiting:
            if (!--plat.count)
            {
                if (plat.sector->floorheight == plat.low)
                    plat.status = PlatState::Up;
                else
                    plat.status = PlatState::Down;
                startSound(reinterpret_cast<Mobj*>(&plat.sector->soundorg),
                           SfxEnum::Pstart);
            }

        case PlatState::InStasis:
            break;
    }
}

//
// Do Platforms
//  "amount" is only used for SOME platforms.
//
int doPlat(Line& line, PlatType type, int amount)
{
    int secnum = -1;
    int rtn = 0;

    // Activate all <type> plats that are PlatState::InStasis
    switch (type)
    {
        case PlatType::PerpetualRaise:
            activateInStasis(line.tag);
            break;
        case PlatType::DownWaitUpStay:
        case PlatType::RaiseAndChange:
        case PlatType::RaiseToNearestAndChange:
        case PlatType::BlazeDWUS:
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
        addThinker(*plat);

        plat->type = type;
        plat->sector = sec;
        plat->sector->specialdata = plat;
        plat->crush = false;
        plat->tag = line.tag;

        switch (type)
        {
            case PlatType::RaiseToNearestAndChange:
                plat->speed = PLATSPEED / 2;
                sec->floorpic = sides[line.sidenum[0]].sector->floorpic;
                plat->high = findNextHighestFloor(*sec, sec->floorheight);
                plat->wait = 0;
                plat->status = PlatState::Up;
                // NO MORE DAMAGE, IF APPLICABLE
                sec->special = 0;

                startSound(reinterpret_cast<Mobj*>(&sec->soundorg), SfxEnum::Stnmov);
                break;

            case PlatType::RaiseAndChange:
                plat->speed = PLATSPEED / 2;
                sec->floorpic = sides[line.sidenum[0]].sector->floorpic;
                plat->high = sec->floorheight + amount * FRACUNIT;
                plat->wait = 0;
                plat->status = PlatState::Up;

                startSound(reinterpret_cast<Mobj*>(&sec->soundorg), SfxEnum::Stnmov);
                break;

            case PlatType::DownWaitUpStay:
                plat->speed = PLATSPEED * 4;
                plat->low = findLowestFloorSurrounding(*sec);

                if (plat->low > sec->floorheight)
                    plat->low = sec->floorheight;

                plat->high = sec->floorheight;
                plat->wait = 35 * PLATWAIT;
                plat->status = PlatState::Down;
                startSound(reinterpret_cast<Mobj*>(&sec->soundorg), SfxEnum::Pstart);
                break;

            case PlatType::BlazeDWUS:
                plat->speed = PLATSPEED * 8;
                plat->low = findLowestFloorSurrounding(*sec);

                if (plat->low > sec->floorheight)
                    plat->low = sec->floorheight;

                plat->high = sec->floorheight;
                plat->wait = 35 * PLATWAIT;
                plat->status = PlatState::Down;
                startSound(reinterpret_cast<Mobj*>(&sec->soundorg), SfxEnum::Pstart);
                break;

            case PlatType::PerpetualRaise:
                plat->speed = PLATSPEED;
                plat->low = findLowestFloorSurrounding(*sec);

                if (plat->low > sec->floorheight)
                    plat->low = sec->floorheight;

                plat->high = findHighestFloorSurrounding(*sec);

                if (plat->high < sec->floorheight)
                    plat->high = sec->floorheight;

                plat->wait = 35 * PLATWAIT;
                plat->status = static_cast<PlatState>(randomness().forPlay() & 1);

                startSound(reinterpret_cast<Mobj*>(&sec->soundorg), SfxEnum::Pstart);
                break;
        }
        addActivePlat(*plat);
    }

    return rtn;
}

void activateInStasis(int tag)
{
    auto& specials = activeSpecials();
    for (auto* plat: specials.activeplats)
        if (plat && plat->tag == tag && plat->status == PlatState::InStasis)
        {
            plat->status = plat->oldstatus;
            plat->stopped = false;
        }
}

void stopPlat(Line& line)
{
    auto& specials = activeSpecials();
    for (auto* plat: specials.activeplats)
        if (plat && plat->status != PlatState::InStasis && plat->tag == line.tag)
        {
            plat->oldstatus = plat->status;
            plat->status = PlatState::InStasis;
            plat->stopped = true;
        }
}

void addActivePlat(Plat& plat)
{
    auto& specials = activeSpecials();
    for (auto*& slot: specials.activeplats)
        if (slot == nullptr)
        {
            slot = &plat;
            return;
        }
    fatalError("Error: addActivePlat: no more plats!");
}

void removeActivePlat(Plat& plat)
{
    auto& specials = activeSpecials();
    for (auto*& slot: specials.activeplats)
        if (&plat == slot)
        {
            slot->sector->specialdata = nullptr;
            removeThinker(*slot);
            slot = nullptr;

            return;
        }
    fatalError("Error: removeActivePlat: can't find plat!");
}
} // namespace Doom
