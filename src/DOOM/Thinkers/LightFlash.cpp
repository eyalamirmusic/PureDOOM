// The broken-light flash thinker's per-tic behaviour. Moved out of vanilla
// p_lights' T_LightFlash so tick() carries the implementation directly rather than
// delegating to a free function.

#include "LightFlash.h"

#include "../Sim/MapTypes.h" // Sector
#include "../Sim/Random.h" // randomness()

namespace Doom
{
void LightFlash::tick()
{
    if (--count)
        return;

    if (sector->lightlevel == maxlight)
    {
        sector->lightlevel = minlight;
        count = (randomness().forPlay() & mintime) + 1;
    }
    else
    {
        sector->lightlevel = maxlight;
        count = (randomness().forPlay() & maxtime) + 1;
    }
}
} // namespace Doom
