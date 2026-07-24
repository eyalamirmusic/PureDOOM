// The strobe-flash thinker's per-tic behaviour. Moved out of vanilla p_lights'
// T_StrobeFlash so tick() carries the implementation directly rather than
// delegating to a free function.

#include "Strobe.h"

#include "../Sim/MapTypes.h" // Sector

namespace Doom
{
void Strobe::tick()
{
    if (--count)
        return;

    if (sector->lightlevel == minlight)
    {
        sector->lightlevel = maxlight;
        count = brighttime;
    }
    else
    {
        sector->lightlevel = minlight;
        count = darktime;
    }
}
} // namespace Doom
