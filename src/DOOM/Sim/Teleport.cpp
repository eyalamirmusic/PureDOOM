#include "Teleport.h"
#include "Level.h"
#include "../Game/PlayerTypes.h"

#include "../doom_config.h"

#include "../doomdef.h"
#include "../p_local.h"
#include "../sounds.h"

#include "../Game/Sound.h"
#include "Mobj.h"
#include "Movement.h"
#include "ThinkerList.h"
namespace Doom
{

int teleport(Line* line, int side, Mobj* thing)
{
    int tag;
    Mobj* m;
    Mobj* fog;
    unsigned an;
    Doom::Thinker* thinker;
    Sector* sector;
    fixed_t oldx;
    fixed_t oldy;
    fixed_t oldz;

    auto& thinkers = thinkerList();

    // don't teleport missiles
    if (thing->flags & MF_MISSILE)
        return 0;

    // Don't teleport if hit back of line,
    //  so you can get out of teleporter.
    if (side == 1)
        return 0;

    tag = line->tag;
    for (int i = 0; i < numsectors; i++)
    {
        if (sectors[i].tag == tag)
        {
            thinker = thinkers.cap.next;
            for (thinker = thinkers.cap.next; thinker != &thinkers.cap;
                 thinker = thinker->next)
            {
                // not a mobj
                if (thinker->kind() != Doom::ThinkerKind::Mobj || thinker->removed)
                    continue;

                m = reinterpret_cast<Mobj*>(thinker);

                // not a teleportman
                if (m->type != MT_TELEPORTMAN)
                    continue;

                sector = m->subsector->sector;
                // wrong sector
                if (sector - sectors != i)
                    continue;

                oldx = thing->x;
                oldy = thing->y;
                oldz = thing->z;

                if (!Doom::teleportMove(thing, m->x, m->y))
                    return 0;

                thing->z = thing->floorz; //fixme: not needed?
                if (thing->player)
                    thing->player->viewz = thing->z + thing->player->viewheight;

                // spawn teleport fog at source and destination
                fog = Doom::spawnMobj(oldx, oldy, oldz, MT_TFOG);
                Doom::startSound(fog, sfx_telept);
                an = m->angle >> ANGLETOFINESHIFT;
                fog = Doom::spawnMobj(m->x + 20 * finecosine[an],
                                  m->y + 20 * finesine[an],
                                  thing->z,
                                  MT_TFOG);

                // emit sound, where?
                Doom::startSound(fog, sfx_telept);

                // don't move for a bit
                if (thing->player)
                    thing->reactiontime = 18;

                thing->angle = m->angle;
                thing->momx = thing->momy = thing->momz = 0;
                return 1;
            }
        }
    }

    return 0;
}
} // namespace Doom
