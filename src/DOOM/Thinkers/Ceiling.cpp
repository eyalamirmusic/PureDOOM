// The moving-ceiling thinker's per-tic behaviour. Moved out of vanilla p_ceilng'
// T_MoveCeiling so tick() carries the implementation directly rather than
// delegating to a free function.

#include "Ceiling.h"

#include "../Sim/Ceilings.h" // removeActiveCeiling
#include "../Sim/Floors.h" // MoveResult
#include "../Sim/MapTypes.h" // Sector
#include "../Game/LevelStats.h" // levelStats()
#include "../Game/Sound.h" // startSound
#include "../Game/SoundData.h" // SfxEnum

namespace Doom
{
void Ceiling::tick()
{
    MoveResult res;

    auto& stats = levelStats();

    switch (direction)
    {
        case 0:
            // IN STASIS
            break;
        case 1:
            // UP
            res = sector->movePlane(speed, topheight, false, 1, direction);

            if (!(stats.leveltime & 7))
            {
                switch (type)
                {
                    case CeilingType::SilentCrushAndRaise:
                        break;
                    case CeilingType::LowerToFloor:
                    case CeilingType::RaiseToHighest:
                    case CeilingType::LowerAndCrush:
                    case CeilingType::CrushAndRaise:
                    case CeilingType::FastCrushAndRaise:
                        startSound(reinterpret_cast<Mobj*>(&sector->soundorg),
                                   SfxEnum::Stnmov);
                        // ?
                        break;
                }
            }

            if (res == MoveResult::PastDest)
            {
                switch (type)
                {
                    case CeilingType::RaiseToHighest:
                        removeActiveCeiling(*this);
                        break;

                    case CeilingType::SilentCrushAndRaise:
                        startSound(reinterpret_cast<Mobj*>(&sector->soundorg),
                                   SfxEnum::Pstop);
                        [[fallthrough]];
                    case CeilingType::FastCrushAndRaise:
                    case CeilingType::CrushAndRaise:
                        direction = -1;
                        break;

                    case CeilingType::LowerToFloor:
                    case CeilingType::LowerAndCrush:
                        break;
                }
            }
            break;

        case -1:
            // DOWN
            res = sector->movePlane(speed, bottomheight, crush, 1, direction);

            if (!(stats.leveltime & 7))
            {
                switch (type)
                {
                    case CeilingType::SilentCrushAndRaise:
                        break;
                    case CeilingType::LowerToFloor:
                    case CeilingType::RaiseToHighest:
                    case CeilingType::LowerAndCrush:
                    case CeilingType::CrushAndRaise:
                    case CeilingType::FastCrushAndRaise:
                        startSound(reinterpret_cast<Mobj*>(&sector->soundorg),
                                   SfxEnum::Stnmov);
                        break;
                }
            }

            if (res == MoveResult::PastDest)
            {
                switch (type)
                {
                    case CeilingType::SilentCrushAndRaise:
                        startSound(reinterpret_cast<Mobj*>(&sector->soundorg),
                                   SfxEnum::Pstop);
                        [[fallthrough]];
                    case CeilingType::CrushAndRaise:
                        speed = CEILSPEED;
                        [[fallthrough]];
                    case CeilingType::FastCrushAndRaise:
                        direction = 1;
                        break;

                    case CeilingType::LowerAndCrush:
                    case CeilingType::LowerToFloor:
                        removeActiveCeiling(*this);
                        break;

                    case CeilingType::RaiseToHighest:
                        break;
                }
            }
            else // ( res != MoveResult::PastDest )
            {
                if (res == MoveResult::Crushed)
                {
                    switch (type)
                    {
                        case CeilingType::SilentCrushAndRaise:
                        case CeilingType::CrushAndRaise:
                        case CeilingType::LowerAndCrush:
                            speed = CEILSPEED / 8;
                            break;

                        case CeilingType::LowerToFloor:
                        case CeilingType::RaiseToHighest:
                        case CeilingType::FastCrushAndRaise:
                            break;
                    }
                }
            }
            break;
    }
}
} // namespace Doom
