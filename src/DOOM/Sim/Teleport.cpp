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
    for (int i = 0; i < level().sectors.size(); i++)
    {
        if (level().sectors[i].tag == tag)
        {
            Thinker* thinker = thinkers.cap.next;
            for (thinker = thinkers.cap.next; thinker != &thinkers.cap;
                 thinker = thinker->next)
            {
                // not a mobj
                if (thinker->kind() != ThinkerKind::Mobj || thinker->removed)
                    continue;

                Mobj* m = reinterpret_cast<Mobj*>(thinker);

                // not a teleportman
                if (m->type != MobjType::Teleportman)
                    continue;

                Sector* sector = m->subsector->sector;
                // wrong sector
                if (sector - level().sectors.data() != i)
                    continue;

                Fixed oldx = thing.x;
                Fixed oldy = thing.y;
                Fixed oldz = thing.z;

                if (!thing.teleportMove(m->x, m->y))
                    return 0;

                thing.z = thing.floorz; //fixme: not needed?
                if (thing.player)
                    thing.player->viewz = thing.z + thing.player->viewheight;

                // spawn teleport fog at source and destination
                Mobj* fog = spawnMobj(oldx, oldy, oldz, MobjType::Tfog);
                startSound(fog, SfxEnum::Telept);
                const auto anFine = m->angle.fineIndex();
                fog = spawnMobj(m->x + 20 * finecosine()[anFine],
                                m->y + 20 * finesine()[anFine],
                                thing.z,
                                MobjType::Tfog);

                // emit sound, where?
                startSound(fog, SfxEnum::Telept);

                // don't move for a bit
                if (thing.player)
                    thing.reactiontime = 18;

                thing.angle = m->angle;
                thing.momx = thing.momy = thing.momz = Fixed {};
                return 1;
            }
        }
    }

    return 0;
}
} // namespace Doom
