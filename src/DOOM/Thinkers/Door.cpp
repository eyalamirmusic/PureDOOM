// The vertical-door thinker's per-tic behaviour. Moved out of vanilla p_doors'
// T_VerticalDoor so tick() carries the implementation directly rather than
// delegating to a free function. (The line-triggered verticalDoor(Line&, Mobj&)
// that opens a door stays in Sim/Doors.cpp - it is a different function.)

#include "Door.h"

#include "../Sim/Floors.h" // movePlane, MoveResult
#include "../Sim/Tick.h" // removeThinker
#include "../Sim/MapTypes.h" // Sector
#include "../Game/Sound.h" // startSound
#include "../Game/SoundData.h" // SfxEnum

namespace Doom
{
void Door::tick()
{
    MoveResult res;

    switch (direction)
    {
        case 0:
            // WAITING
            if (!--topcountdown)
            {
                switch (type)
                {
                    case DoorType::BlazeRaise:
                        direction = -1; // time to go back down
                        startSound(reinterpret_cast<Mobj*>(&sector->soundorg),
                                   SfxEnum::Bdcls);
                        break;

                    case DoorType::Normal:
                        direction = -1; // time to go back down
                        startSound(reinterpret_cast<Mobj*>(&sector->soundorg),
                                   SfxEnum::Dorcls);
                        break;

                    case DoorType::Close30ThenOpen:
                        direction = 1;
                        startSound(reinterpret_cast<Mobj*>(&sector->soundorg),
                                   SfxEnum::Doropn);
                        break;

                    case DoorType::Close:
                    case DoorType::Open:
                    case DoorType::RaiseIn5Mins:
                    case DoorType::BlazeOpen:
                    case DoorType::BlazeClose:
                        break;
                }
            }
            break;

        case 2:
            //  INITIAL WAIT
            if (!--topcountdown)
            {
                switch (type)
                {
                    case DoorType::RaiseIn5Mins:
                        direction = 1;
                        type = DoorType::Normal;
                        startSound(reinterpret_cast<Mobj*>(&sector->soundorg),
                                   SfxEnum::Doropn);
                        break;

                    case DoorType::Normal:
                    case DoorType::Close30ThenOpen:
                    case DoorType::Close:
                    case DoorType::Open:
                    case DoorType::BlazeRaise:
                    case DoorType::BlazeOpen:
                    case DoorType::BlazeClose:
                        break;
                }
            }
            break;

        case -1:
            // DOWN
            res =
                movePlane(*sector, speed, sector->floorheight, false, 1, direction);
            if (res == MoveResult::PastDest)
            {
                switch (type)
                {
                    case DoorType::BlazeRaise:
                    case DoorType::BlazeClose:
                        sector->specialdata = nullptr;
                        removeThinker(*this); // unlink and free
                        startSound(reinterpret_cast<Mobj*>(&sector->soundorg),
                                   SfxEnum::Bdcls);
                        break;

                    case DoorType::Normal:
                    case DoorType::Close:
                        sector->specialdata = nullptr;
                        removeThinker(*this); // unlink and free
                        break;

                    case DoorType::Close30ThenOpen:
                        direction = 0;
                        topcountdown = 35 * 30;
                        break;

                    case DoorType::Open:
                    case DoorType::RaiseIn5Mins:
                    case DoorType::BlazeOpen:
                        break;
                }
            }
            else if (res == MoveResult::Crushed)
            {
                switch (type)
                {
                    case DoorType::BlazeClose:
                    case DoorType::Close: // DO NOT GO BACK UP!
                        break;

                    case DoorType::Normal:
                    case DoorType::Close30ThenOpen:
                    case DoorType::Open:
                    case DoorType::RaiseIn5Mins:
                    case DoorType::BlazeRaise:
                    case DoorType::BlazeOpen:
                        direction = 1;
                        startSound(reinterpret_cast<Mobj*>(&sector->soundorg),
                                   SfxEnum::Doropn);
                        break;
                }
            }
            break;

        case 1:
            // UP
            res = movePlane(*sector, speed, topheight, false, 1, direction);

            if (res == MoveResult::PastDest)
            {
                switch (type)
                {
                    case DoorType::BlazeRaise:
                    case DoorType::Normal:
                        direction = 0; // wait at top
                        topcountdown = topwait;
                        break;

                    case DoorType::Close30ThenOpen:
                    case DoorType::BlazeOpen:
                    case DoorType::Open:
                        sector->specialdata = nullptr;
                        removeThinker(*this); // unlink and free
                        break;

                    case DoorType::Close:
                    case DoorType::RaiseIn5Mins:
                    case DoorType::BlazeClose:
                        break;
                }
            }
            break;
    }
}
} // namespace Doom
