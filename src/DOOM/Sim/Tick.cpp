// Rewritten out of vanilla p_tick into namespace Doom.
//
// The thinker list (add/remove/run) and the per-tic ticker that thinks each
// player and runs the thinkers, specials and respawns. Golden-neutral - this is the
// heartbeat every demo tic runs through.

#include "../Host/Platform.h"

#include "../Game/MapSpawns.h"
#include "SimDefs.h"

#include "../Game/DemoState.h"
#include "../Game/GameSession.h"
#include "../Game/LevelStats.h"
#include "../Game/OverlayState.h"
#include "../Game/PlayerState.h"
#include "../Game/RefreshFlags.h"
#include "ThinkerList.h"
#include "Tick.h"

#include "Specials.h"

#include "Mobj.h"

#include "Player.h"
namespace Doom
{
// Every thinker's storage comes from the embedder's hook, which is what levelAlloc
// existed to do before the ThinkerList owned these blocks (Sim/Thinker.h says why
// the global operator new will not do). The list-threading half of levelAlloc is
// gone with the LevelPool: the OwnedVector is the ownership now, so a block cannot
// be allocated and left unregistered.
//
// No zeroing here, where levelAlloc memset the block. It is not needed and its
// absence is not a behaviour change: every thinker is created by
// `new T()` (OwnedVector::createDerived), and a T whose default constructor is
// implicit and non-trivial - which all nine are, being polymorphic with default
// member initializers - is *value*-initialized by that syntax, so the object is
// zeroed whole, padding included, before its members are initialized. The padding
// matters because SaveGame memcpy's these structs to disk entire.
void* Thinker::operator new(std::size_t size)
{
    return host().malloc(static_cast<int>(size));
}

void Thinker::operator delete(void* block)
{
    host().free(block);
}

//
// removeThinker
// Deallocation is lazy -- it will not actually be freed
// until its thinking turn comes up.
//
void removeThinker(Thinker& thinker)
{
    // Deallocation is lazy: mark it, and runThinkers frees it when its turn next
    // comes up. Was `function.acv = (actionf_v) -1`.
    thinker.removed = true;
}

//
// runThinkers
//
void runThinkers()
{
    auto& thinkers = thinkerList();

    // By index, re-reading size() every iteration, and never through a reference
    // into the vector's buffer: a thinker's tick() spawns thinkers, which append
    // here and can reallocate. Binding to *thinkers[i] is safe because that is the
    // object, not the slot, and the objects never move.
    //
    // The three orderings this reproduces, each pinned by the demo goldens: a
    // thinker spawned during this walk runs in this same tic (it lands at the back,
    // which the cursor has not reached); a removed thinker is freed when the cursor
    // arrives, which may be this tic or the next depending on where it sits; and a
    // stopped thinker (a crusher/lift in stasis - vanilla's null function) keeps its
    // place without acting.
    for (auto i = 0; i < thinkers.size();)
    {
        auto& thinker = *thinkers[i];

        if (thinker.removed)
        {
            // Order-preserving erase, and the OwningPointer in the slot is what runs
            // the destructor and returns the storage. Never swap-and-pop: the order
            // is the simulation.
            thinkers.removeAt(i);
            continue;
        }

        if (!thinker.stopped)
            thinker.tick();

        ++i;
    }
}

//
// ticker
//
void ticker()
{
    // run the tic
    if (refreshFlags().paused)
        return;

    auto& players_ = playerState();

    // pause if in menu and at least one tic has been run
    if (!gameSession().netgame && overlayState().menuactive
        && !demoState().demoplayback
        && players_.players[players_.consoleplayer].viewz != Fixed {1})
    {
        return;
    }

    for (auto i = 0; i < MAXPLAYERS; i++)
        if (players_.playeringame[i])
            players_.players[i].think();

    runThinkers();
    updateSpecials();
    respawnSpecials();

    // for par times
    levelStats().leveltime++;
}
} // namespace Doom
