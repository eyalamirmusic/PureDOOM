// Rewritten out of vanilla p_tick into namespace Doom.
//
// The thinker list (init/add/remove/run) and the per-tic Doom::ticker that thinks each
// player and runs the thinkers, specials and respawns. The run loop dispatches
// through the thinker function-pointer union, so the T_/Doom::mobjThinker addresses it
// stores stay global. p_tick.cpp shims the vanilla names and owns leveltime and
// thinkercap. Golden-neutral - this is the heartbeat every demo tic runs through.

#include "../Host/Platform.h"

#include "../Game/MapSpawns.h"
#include "SimDefs.h"

#include "../Game/DemoState.h"
#include "../Game/GameSession.h"
#include "../Game/LevelStats.h"
#include "../Game/OverlayState.h"
#include "../Game/PlayerState.h"
#include "../Game/RefreshFlags.h"
#include "LevelPool.h"
#include "ThinkerList.h"
#include "Tick.h"

// The thinker functions stay global (p_saveg identity); declared so the spawners
#include "Specials.h"
// can store their address.
#include "Mobj.h"

#include "Player.h"
namespace Doom
{
// Forward declarations so the file's own call order needs no rearranging.
void initThinkers();
void addThinker(Doom::Thinker* thinker);
void removeThinker(Doom::Thinker* thinker);
void runThinkers();
void ticker();

// The level-allocation pool. Each block carries a small header linking it into an
// intrusive list, so a level reset frees every block malloc gave out - live
// thinkers and the marked-but-orphaned alike - where Z_FreeTags(PU_LEVEL) once
// swept the whole tag. The header is two pointers, so the returned block stays
// 16-byte aligned, more than Mobj asks for.
// LevelChunk and the list head now live on the Engine (Sim/LevelPool.h, moved by the
// file-scope-statics sweep - REFACTOR.md, Step 5); levelAlloc, levelFree and freeLevelAllocations
// each hoist levelPool() once and reach the list head through it (pool.head), so the pool is owned
// per-Engine, rather than through a file-scope reference alias (REFACTOR.md, Step 9 strand (a)).
// Read by no other file.

void* levelAlloc(int size)
{
    auto& pool = levelPool();

    int total = static_cast<int>(sizeof(LevelChunk)) + size;
    LevelChunk* chunk = static_cast<LevelChunk*>(doom_malloc(total));
    doom_memset(chunk, 0, total);

    chunk->prev = nullptr;
    chunk->next = pool.head;
    if (pool.head)
        pool.head->prev = chunk;
    pool.head = chunk;

    return static_cast<void*>(chunk + 1);
}

void levelFree(void* block)
{
    if (!block)
        return;

    auto& pool = levelPool();

    LevelChunk* chunk = static_cast<LevelChunk*>(block) - 1;
    if (chunk->prev)
        chunk->prev->next = chunk->next;
    else
        pool.head = chunk->next;
    if (chunk->next)
        chunk->next->prev = chunk->prev;

    doom_free(chunk);
}

// Every level allocation is a Thinker (mobj or special), so a chunk's payload can
// be destroyed as one. The virtual destructor is defaulted and the fields are all
// trivially destructible, so this frees no resources - it is here because ending
// the lifetime of a polymorphic object by releasing its storage is only well-defined
// once its destructor has run.
static void destroyThinker(Doom::Thinker* thinker)
{
    thinker->~Thinker();
    levelFree(thinker);
}

void freeLevelAllocations()
{
    auto& pool = levelPool();

    LevelChunk* chunk = pool.head;
    while (chunk)
    {
        LevelChunk* next = chunk->next;
        reinterpret_cast<Doom::Thinker*>(chunk + 1)->~Thinker();
        doom_free(chunk);
        chunk = next;
    }
    pool.head = nullptr;
}

void initThinkers()
{
    auto& thinkers = thinkerList();
    thinkers.cap.prev = thinkers.cap.next = &thinkers.cap;
}

//
// addThinker
// Adds a new thinker at the end of the list.
//
void addThinker(Doom::Thinker* thinker)
{
    auto& thinkers = thinkerList();
    thinkers.cap.prev->next = thinker;
    thinker->next = &thinkers.cap;
    thinker->prev = thinkers.cap.prev;
    thinkers.cap.prev = thinker;
}

//
// removeThinker
// Deallocation is lazy -- it will not actually be freed
// until its thinking turn comes up.
//
void removeThinker(Doom::Thinker* thinker)
{
    // Deallocation is lazy: mark it, and runThinkers frees it when its turn next
    // comes up. Was `function.acv = (actionf_v) -1`.
    thinker->removed = true;
}

//
// runThinkers
//
void runThinkers()
{
    auto& thinkers = thinkerList();
    Doom::Thinker* currentthinker = thinkers.cap.next;
    while (currentthinker != &thinkers.cap)
    {
        if (currentthinker->removed)
        {
            // Time to remove it. Vanilla advanced by reading currentthinker->next
            // *after* Z_Free, which the zone tolerated because a freed block kept
            // its bytes until reused; a real free() unmaps them, so capture next
            // before releasing (the unlink leaves currentthinker->next intact, so
            // this is the same value vanilla read - only sooner).
            Doom::Thinker* next = currentthinker->next;
            currentthinker->next->prev = currentthinker->prev;
            currentthinker->prev->next = currentthinker->next;
            destroyThinker(currentthinker);
            currentthinker = next;
        }
        else
        {
            // A stopped thinker (a crusher/lift in stasis - vanilla's null function)
            // stays on the list but does not act.
            if (!currentthinker->stopped)
                currentthinker->tick();
            // Advance after the think, so a mobj its thinker just spawned (linked
            // at the tail, i.e. onto currentthinker->next when it was last) runs
            // this same tic, as vanilla does.
            currentthinker = currentthinker->next;
        }
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
        && players_.players[players_.consoleplayer].viewz != fixed_t {1})
    {
        return;
    }

    for (int i = 0; i < MAXPLAYERS; i++)
        if (players_.playeringame[i])
            playerThink(players_.players[i]);

    runThinkers();
    Doom::updateSpecials();
    Doom::respawnSpecials();

    // for par times
    levelStats().leveltime++;
}
} // namespace Doom
