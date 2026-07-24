// The moving-floor thinker's per-tic behaviour. Moved out of vanilla p_floor'
// T_MoveFloor so tick() carries the implementation directly rather than delegating
// to a free function. (Sector::movePlane and the EV_ handlers stay in Sim/Floors.cpp.)

#include "FloorMove.h"

#include "../Sim/Floors.h" // MoveResult
#include "../Sim/Tick.h" // removeThinker
#include "../Sim/MapTypes.h" // Sector
#include "../Game/LevelStats.h" // levelStats()
#include "../Game/Sound.h" // startSound
#include "../Game/SoundData.h" // SfxEnum

namespace Doom
{
void FloorMove::tick()
{
    MoveResult res = sector->movePlane(speed, floordestheight, crush, 0, direction);

    if (!(levelStats().leveltime & 7))
        startSound(reinterpret_cast<Mobj*>(&sector->soundorg), SfxEnum::Stnmov);

    if (res == MoveResult::PastDest)
    {
        sector->specialdata = nullptr;

        if (direction == 1)
        {
            switch (type)
            {
                case FloorType::DonutRaise:
                    sector->special = newspecial;
                    sector->floorpic = texture;
                    break;

                case FloorType::LowerFloor:
                case FloorType::LowerFloorToLowest:
                case FloorType::TurboLower:
                case FloorType::RaiseFloor:
                case FloorType::RaiseFloorToNearest:
                case FloorType::RaiseToTexture:
                case FloorType::LowerAndChange:
                case FloorType::RaiseFloor24:
                case FloorType::RaiseFloor24AndChange:
                case FloorType::RaiseFloorCrush:
                case FloorType::RaiseFloorTurbo:
                case FloorType::RaiseFloor512:
                    break;
            }
        }
        else if (direction == -1)
        {
            switch (type)
            {
                case FloorType::LowerAndChange:
                    sector->special = newspecial;
                    sector->floorpic = texture;
                    break;

                case FloorType::LowerFloor:
                case FloorType::LowerFloorToLowest:
                case FloorType::TurboLower:
                case FloorType::RaiseFloor:
                case FloorType::RaiseFloorToNearest:
                case FloorType::RaiseToTexture:
                case FloorType::RaiseFloor24:
                case FloorType::RaiseFloor24AndChange:
                case FloorType::RaiseFloorCrush:
                case FloorType::RaiseFloorTurbo:
                case FloorType::DonutRaise:
                case FloorType::RaiseFloor512:
                    break;
            }
        }
        removeThinker(*this);

        startSound(reinterpret_cast<Mobj*>(&sector->soundorg), SfxEnum::Pstop);
    }
}
} // namespace Doom
