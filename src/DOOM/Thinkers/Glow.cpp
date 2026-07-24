// The glowing-light thinker's per-tic behaviour. Moved out of vanilla p_lights'
// T_Glow so tick() carries the implementation directly rather than delegating to a
// free function.

#include "Glow.h"

#include "../Sim/MapTypes.h" // Sector

namespace Doom
{
void Glow::tick()
{
    switch (direction)
    {
        case -1:
            // DOWN
            sector->lightlevel -= GLOWSPEED;
            if (sector->lightlevel <= minlight)
            {
                sector->lightlevel += GLOWSPEED;
                direction = 1;
            }
            break;

        case 1:
            // UP
            sector->lightlevel += GLOWSPEED;
            if (sector->lightlevel >= maxlight)
            {
                sector->lightlevel -= GLOWSPEED;
                direction = -1;
            }
            break;
    }
}
} // namespace Doom
