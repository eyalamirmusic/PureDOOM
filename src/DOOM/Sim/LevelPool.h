#pragma once

namespace Doom
{
// The intrusive list node for the level-scoped malloc pool that replaced the zone allocator
// (REFACTOR.md, Step 4): every level allocation - a mobj or a thinker special - is one of these,
// carrying a two-pointer header so the block it returns stays 16-byte aligned. Sim/Tick's levelAlloc
// pushes onto the list, levelFree unlinks, and freeLevelAllocations releases the whole list on level
// reload (the list, not a thinkercap walk, is what makes it leak-free - unArchiveThinkers orphans
// without freeing).
struct LevelChunk
{
    LevelChunk* next;
    LevelChunk* prev;
};

// The head of that list. Moved into the Engine by the file-scope-statics sweep (REFACTOR.md, Step 5)
// so the level's allocation pool is owned per-Engine rather than by the process - a step toward the
// engine being *constructed* rather than booted. It was Sim/Tick's own file-local (anonymous-
// namespace) global, read by no other file; levelAlloc, levelFree and freeLevelAllocations each
// hoist levelPool() once and reach head through it (pool.head), rather than through a file-scope
// reference alias (REFACTOR.md, Step 9 strand (a)) - it stays a genuine reference at each hoist
// site, since pool.head is written directly through the struct rather than copied. Live
// simulation-golden-covered - every mobj the demos spawn is allocated through this list.
// The pool *owns* every block levelAlloc handed out, so it releases them in a
// destructor rather than waiting to be told (REFACTOR.md, Step 9 strand (b)). It
// was a bare `{ LevelChunk* head; }` until then, which was safe only for as long as
// the Engine outlived the process: loadLevel calls freeLevelAllocations on
// reload, but nothing did so on teardown, so destroying an Engine with a level
// loaded leaked every mobj and thinker special on the list. That became reachable
// the moment strand (a) made the Engine constructible - resetEngine() dropped the
// instance and the whole pool with it, measured at 120 blocks after one E1M1 load.
//
// The list cannot become a container, and that is the reason this is a destructor
// and not an Vector: the blocks are variable-sized, hold polymorphic Thinkers
// whose addresses Sim/SaveGame serialises and the thinker list threads, and so can
// never be moved or relocated. RAII here means owning the release, not owning the
// layout.
struct LevelPool
{
    LevelChunk* head = nullptr;

    LevelPool() = default;
    ~LevelPool();

    // Raw ownership, so copying would double-free. Nothing copies an Engine today;
    // this makes a future attempt a compile error rather than a heap corruption.
    LevelPool(const LevelPool&) = delete;
    LevelPool& operator=(const LevelPool&) = delete;

    // Runs every payload's destructor and frees every block, leaving the pool empty.
    // Both the level-reload path (freeLevelAllocations) and the destructor go through
    // this, so teardown and reload cannot drift apart.
    void releaseAll();
};

// The one LevelPool, a view onto the Engine's member - the same pattern as the other clusters.
LevelPool& levelPool();
} // namespace Doom
