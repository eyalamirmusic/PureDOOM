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
// doPlat and stopPlat are Line methods now (declared in MapTypes.h). activateInStasis
// keys off a plain tag with no owning object, and addActivePlat/removeActivePlat
// insert/remove a Plat in the level's activeplats slot table - the same registry role
// addThinker/removeThinker keep as free functions - so these three stay free.
void activateInStasis(int tag);
void addActivePlat(Plat& plat);
void removeActivePlat(Plat& plat);

//
// Do Platforms
//  "amount" is only used for SOME platforms.
//
int Line::doPlat(PlatType type, int amount)
{
    int secnum = -1;
    int rtn = 0;

    // Activate all <type> plats that are PlatState::InStasis
    switch (type)
    {
        case PlatType::PerpetualRaise:
            activateInStasis(tag);
            break;
        case PlatType::DownWaitUpStay:
        case PlatType::RaiseAndChange:
        case PlatType::RaiseToNearestAndChange:
        case PlatType::BlazeDWUS:
            break;
    }

    while ((secnum = findSectorFromLineTag(secnum)) >= 0)
    {
        Sector* sec = &level().sectors[secnum];

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
        plat->tag = tag;

        switch (type)
        {
            case PlatType::RaiseToNearestAndChange:
                plat->speed = PLATSPEED / 2;
                sec->floorpic = level().sides[sidenum[0]].sector->floorpic;
                plat->high = sec->findNextHighestFloor(sec->floorheight);
                plat->wait = 0;
                plat->status = PlatState::Up;
                // NO MORE DAMAGE, IF APPLICABLE
                sec->special = 0;

                startSound(reinterpret_cast<Mobj*>(&sec->soundorg), SfxEnum::Stnmov);
                break;

            case PlatType::RaiseAndChange:
                plat->speed = PLATSPEED / 2;
                sec->floorpic = level().sides[sidenum[0]].sector->floorpic;
                plat->high = sec->floorheight + amount * FRACUNIT;
                plat->wait = 0;
                plat->status = PlatState::Up;

                startSound(reinterpret_cast<Mobj*>(&sec->soundorg), SfxEnum::Stnmov);
                break;

            case PlatType::DownWaitUpStay:
                plat->speed = PLATSPEED * 4;
                plat->low = sec->findLowestFloorSurrounding();

                if (plat->low > sec->floorheight)
                    plat->low = sec->floorheight;

                plat->high = sec->floorheight;
                plat->wait = 35 * PLATWAIT;
                plat->status = PlatState::Down;
                startSound(reinterpret_cast<Mobj*>(&sec->soundorg), SfxEnum::Pstart);
                break;

            case PlatType::BlazeDWUS:
                plat->speed = PLATSPEED * 8;
                plat->low = sec->findLowestFloorSurrounding();

                if (plat->low > sec->floorheight)
                    plat->low = sec->floorheight;

                plat->high = sec->floorheight;
                plat->wait = 35 * PLATWAIT;
                plat->status = PlatState::Down;
                startSound(reinterpret_cast<Mobj*>(&sec->soundorg), SfxEnum::Pstart);
                break;

            case PlatType::PerpetualRaise:
                plat->speed = PLATSPEED;
                plat->low = sec->findLowestFloorSurrounding();

                if (plat->low > sec->floorheight)
                    plat->low = sec->floorheight;

                plat->high = sec->findHighestFloorSurrounding();

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

void Line::stopPlat()
{
    auto& specials = activeSpecials();
    for (auto* plat: specials.activeplats)
        if (plat && plat->status != PlatState::InStasis && plat->tag == tag)
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
