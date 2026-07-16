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

#include "LevelPool.h"
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

// The level-allocation pool. Each block carries a small header linking it into an
// intrusive list, so a level reset frees every block malloc gave out - live
// thinkers and the marked-but-orphaned alike - where Z_FreeTags(PU_LEVEL) once
// swept the whole tag. The header is two pointers, so the returned block stays
// 16-byte aligned, more than mobj_t asks for.
// LevelChunk and the list head now live on the Engine (Sim/LevelPool.h, moved by the
// file-scope-statics sweep - REFACTOR.md, Step 5); the vanilla name levelChunks is a reference onto
// that member, so the pool is owned per-Engine. Read by no other file.
static LevelChunk*& levelChunks = levelPool().head;

void* levelAlloc(int size)
{
    int total = (int) sizeof(LevelChunk) + size;
    LevelChunk* chunk = (LevelChunk*) doom_malloc(total);
    doom_memset(chunk, 0, total);

    chunk->prev = 0;
    chunk->next = levelChunks;
    if (levelChunks)
        levelChunks->prev = chunk;
    levelChunks = chunk;

    return (void*) (chunk + 1);
}

void levelFree(void* block)
{
    if (!block)
        return;

    LevelChunk* chunk = (LevelChunk*) block - 1;
    if (chunk->prev)
        chunk->prev->next = chunk->next;
    else
        levelChunks = chunk->next;
    if (chunk->next)
        chunk->next->prev = chunk->prev;

    doom_free(chunk);
}

void freeLevelAllocations(void)
{
    LevelChunk* chunk = levelChunks;
    while (chunk)
    {
        LevelChunk* next = chunk->next;
        doom_free(chunk);
        chunk = next;
    }
    levelChunks = 0;
}

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
            // Time to remove it. Vanilla advanced by reading currentthinker->next
            // *after* Z_Free, which the zone tolerated because a freed block kept
            // its bytes until reused; a real free() unmaps them, so capture next
            // before releasing (the unlink leaves currentthinker->next intact, so
            // this is the same value vanilla read - only sooner).
            thinker_t* next = currentthinker->next;
            currentthinker->next->prev = currentthinker->prev;
            currentthinker->prev->next = currentthinker->next;
            levelFree(currentthinker);
            currentthinker = next;
        }
        else
        {
            if (currentthinker->function.acp1)
                currentthinker->function.acp1(currentthinker);
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
