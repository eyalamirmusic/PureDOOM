#include "Teleport.h"
#include "Level.h"
#include "../Game/PlayerTypes.h"

#include "../Host/Platform.h"

#include "../Game/GameDefs.h"
#include "SimDefs.h"
#include "../Game/SoundData.h"

#include "../Game/Sound.h"
#include "Mobj.h"
#include "Movement.h"
#include "ThinkerList.h"
namespace Doom
{

int Line::teleport(int side, Mobj& thing)
{
    auto& thinkers = thinkerList();

    // don't teleport missiles
    if (hasFlag(thing.flags, MobjFlag::Missile))
        return 0;

    // Don't teleport if hit back of line,
    //  so you can get out of teleporter.
    if (side == 1)
        return 0;

    // `tag` here is this line's own tag member.
    for (auto& destination: level().sectors)
    {
        if (destination.tag == tag)
        {
            // By index, not a range-for: the body spawns teleport fog, which appends
            // to this very list and can reallocate its buffer. Every path that gets
            // that far returns before the next iteration, so a range-for would be
            // correct today and silently undefined the moment one of those returns
            // goes away. An index survives an append; an iterator does not. (`m`
            // itself stays valid either way - the vector owns pointers, so the mobjs
            // never move.)
            for (auto i = 0; i < thinkers.size(); i++)
            {
                auto* thinker = thinkers[i].get();

                // not a mobj
                auto* m = thinker->asMobj();
                if (!m || thinker->removed)
                    continue;

                // not a teleportman
                if (m->type != MobjType::Teleportman)
                    continue;

                // wrong sector
                if (m->subsector->sector != &destination)
                    continue;

                auto oldx = thing.pos.x;
                auto oldy = thing.pos.y;
                auto oldz = thing.pos.z;

                if (!thing.teleportMove(m->pos.xy()))
                    return 0;

                thing.pos.z = thing.floorz; //fixme: not needed?
                if (thing.player)
                    thing.player->viewz = thing.pos.z + thing.player->viewheight;

                // spawn teleport fog at source and destination
                auto* fog = spawnMobj({oldx, oldy, oldz}, MobjType::Tfog);
                startSound(fog, SfxEnum::Telept);
                const auto anFine = m->angle.fineIndex();
                fog = spawnMobj({m->pos.x + 20 * finecosine()[anFine],
                                 m->pos.y + 20 * finesine()[anFine],
                                 thing.pos.z},
                                MobjType::Tfog);

                // emit sound, where?
                startSound(fog, SfxEnum::Telept);

                // don't move for a bit
                if (thing.player)
                    thing.reactiontime = 18;

                thing.angle = m->angle;
                thing.mom = {};
                return 1;
            }
        }
    }

    return 0;
}
} // namespace Doom
