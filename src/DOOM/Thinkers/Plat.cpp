// The platform thinker's per-tic behaviour. Moved out of vanilla p_plats'
// T_PlatRaise so tick() carries the implementation directly rather than delegating
// to a free function.

#include "Plat.h"

#include "../Sim/Plats.h" // removeActivePlat
#include "../Sim/Floors.h" // MoveResult
#include "../Sim/MapTypes.h" // Sector
#include "../Game/LevelStats.h" // levelStats()
#include "../Game/Sound.h" // startSound
#include "../Game/SoundData.h" // SfxEnum

namespace Doom
{
void Plat::tick()
{
    MoveResult res;

    switch (status)
    {
        case PlatState::Up:
            res = sector->movePlane(speed, high, crush, 0, 1);

            if (type == PlatType::RaiseAndChange
                || type == PlatType::RaiseToNearestAndChange)
            {
                if (!(levelStats().leveltime & 7))
                    startSound(reinterpret_cast<Mobj*>(&sector->soundorg),
                               SfxEnum::Stnmov);
            }

            if (res == MoveResult::Crushed && (!crush))
            {
                count = wait;
                status = PlatState::Down;
                startSound(reinterpret_cast<Mobj*>(&sector->soundorg),
                           SfxEnum::Pstart);
            }
            else
            {
                if (res == MoveResult::PastDest)
                {
                    count = wait;
                    status = PlatState::Waiting;
                    startSound(reinterpret_cast<Mobj*>(&sector->soundorg),
                               SfxEnum::Pstop);

                    switch (type)
                    {
                        case PlatType::BlazeDWUS:
                        case PlatType::DownWaitUpStay:
                            removeActivePlat(*this);
                            break;

                        case PlatType::RaiseAndChange:
                        case PlatType::RaiseToNearestAndChange:
                            removeActivePlat(*this);
                            break;

                        case PlatType::PerpetualRaise:
                            break;
                    }
                }
            }
            break;

        case PlatState::Down:
            res = sector->movePlane(speed, low, false, 0, -1);

            if (res == MoveResult::PastDest)
            {
                count = wait;
                status = PlatState::Waiting;
                startSound(reinterpret_cast<Mobj*>(&sector->soundorg),
                           SfxEnum::Pstop);
            }
            break;

        case PlatState::Waiting:
            if (!--count)
            {
                if (sector->floorheight == low)
                    status = PlatState::Up;
                else
                    status = PlatState::Down;
                startSound(reinterpret_cast<Mobj*>(&sector->soundorg),
                           SfxEnum::Pstart);
            }

        case PlatState::InStasis:
            break;
    }
}
} // namespace Doom
