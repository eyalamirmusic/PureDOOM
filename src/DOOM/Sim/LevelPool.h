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
// namespace) global, read by no other file; the vanilla name becomes a reference onto head. Live
// simulation-golden-covered - every mobj the demos spawn is allocated through this list.
struct LevelPool
{
    LevelChunk* head = nullptr;
};

// The one LevelPool, a view onto the Engine's member - the same pattern as the other clusters.
LevelPool& levelPool();
} // namespace Doom
