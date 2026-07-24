// The fire-flicker light thinker's per-tic behaviour. Moved out of vanilla
// p_lights' T_FireFlicker so tick() carries the implementation directly rather
// than delegating to a free function.

#include "FireFlicker.h"

#include "../Sim/MapTypes.h" // Sector
#include "../Sim/Random.h" // randomness()

namespace Doom
{
void FireFlicker::tick()
{
    if (--count)
        return;

    int amount = (randomness().forPlay() & 3) * 16;

    if (sector->lightlevel - amount < minlight)
        sector->lightlevel = minlight;
    else
        sector->lightlevel = maxlight - amount;

    count = 4;
}
} // namespace Doom
