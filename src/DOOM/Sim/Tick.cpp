// Rewritten out of vanilla p_tick into namespace Doom.
//
// The thinker list (init/add/remove/run) and the per-tic P_Ticker that thinks each
// player and runs the thinkers, specials and respawns. The run loop dispatches
// through the thinker function-pointer union, so the T_/P_MobjThinker addresses it
// stores stay global. p_tick.cpp shims the vanilla names and owns leveltime and
// thinkercap. Golden-neutral - this is the heartbeat every demo tic runs through.

#include "../doom_config.h"

#include "../doomstat.h"
#include "../p_local.h"
#include "../z_zone.h"

#include "Tick.h"

// The thinker functions stay global (p_saveg identity); declared so the spawners
// can store their address.

namespace Doom
{
// Forward declarations so the file's own call order needs no rearranging.
void initThinkers(void);
void addThinker(thinker_t* thinker);
void removeThinker(thinker_t* thinker);
void runThinkers(void);
void ticker(void);

void initThinkers(void)
{
    thinkercap.prev = thinkercap.next = &thinkercap;
}

//
// addThinker
// Adds a new thinker at the end of the list.
//
void addThinker(thinker_t* thinker)
{
    thinkercap.prev->next = thinker;
    thinker->next = &thinkercap;
    thinker->prev = thinkercap.prev;
    thinkercap.prev = thinker;
}

//
// removeThinker
// Deallocation is lazy -- it will not actually be freed
// until its thinking turn comes up.
//
void removeThinker(thinker_t* thinker)
{
    // FIXME: NOP.
    thinker->function.acv = (actionf_v) (-1);
}

//
// runThinkers
//
void runThinkers(void)
{
    thinker_t* currentthinker;

    currentthinker = thinkercap.next;
    while (currentthinker != &thinkercap)
    {
        if (currentthinker->function.acv == (actionf_v) (-1))
        {
            // time to remove it
            currentthinker->next->prev = currentthinker->prev;
            currentthinker->prev->next = currentthinker->next;
            Z_Free(currentthinker);
        }
        else
        {
            if (currentthinker->function.acp1)
                currentthinker->function.acp1(currentthinker);
        }
        currentthinker = currentthinker->next;
    }
}

//
// ticker
//
void ticker(void)
{
    int i;

    // run the tic
    if (paused)
        return;

    // pause if in menu and at least one tic has been run
    if (!netgame && menuactive && !demoplayback && players[consoleplayer].viewz != 1)
    {
        return;
    }

    for (i = 0; i < MAXPLAYERS; i++)
        if (playeringame[i])
            P_PlayerThink(&players[i]);

    runThinkers();
    P_UpdateSpecials();
    P_RespawnSpecials();

    // for par times
    leveltime++;
}
} // namespace Doom
