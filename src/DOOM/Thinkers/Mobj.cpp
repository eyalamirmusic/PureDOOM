// The map-object thinker's per-tic behaviour. Moved out of vanilla p_mobj's
// P_MobjThinker so tick() carries the implementation directly rather than
// delegating to a free function. The heavy lifting - xyMovement, zMovement,
// setMobjState and nightmareRespawn - stays in Sim/Mobj.cpp and is reached through
// Sim/Mobj.h; this is the momentum/state-cycling/respawn skeleton that calls them.

#include "Mobj.h"

#include "../Sim/Mobj.h" // xyMovement, zMovement, nightmareRespawn, setMobjState
#include "../Sim/Random.h" // randomness()
#include "../Game/GameSession.h" // gameSession()
#include "../Game/LevelStats.h" // levelStats()

namespace Doom
{
void Mobj::tick()
{
    // momentum movement
    if (momx || momy || (hasFlag(flags, MobjFlag::SkullFly)))
    {
        xyMovement(*this);

        // FIXME: decent NOP/0/Nil function pointer please.
        if (removed)
            return; // mobj was removed
    }
    if ((z != floorz) || momz)
    {
        zMovement(*this);

        // FIXME: decent NOP/0/Nil function pointer please.
        if (removed)
            return; // mobj was removed
    }

    // cycle through states,
    // calling action functions at transitions
    if (tics != -1)
    {
        tics--;

        // you can cycle through multiple states in a tic
        if (!tics)
            if (!setMobjState(*this, state->nextstate))
                return; // freed itself
    }
    else
    {
        // check for nightmare respawn
        if (!(hasFlag(flags, MobjFlag::CountKill)))
            return;

        if (!gameSession().respawnmonsters)
            return;

        movecount++;

        if (movecount < 12 * 35)
            return;

        if (levelStats().leveltime & 31)
            return;

        if (randomness().forPlay() > 4)
            return;

        nightmareRespawn(*this);
    }
}
} // namespace Doom
