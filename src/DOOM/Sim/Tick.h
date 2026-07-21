#pragma once

#include "ActionFunc.h" // Doom::Thinker

namespace Doom
{
// The thinker list and the per-tic ticker; p_tick.cpp keeps the vanilla names.
void initThinkers();
void addThinker(Thinker& thinker);
void removeThinker(Thinker& thinker);
void runThinkers();
void ticker();

// Level-scoped allocation for mobjs and the thinker specials - what the zone's
// PU_LEVEL / PU_LEVSPEC tags used to serve. Every block is tracked in an intrusive
// list so freeLevelAllocations can reclaim the whole level at once (the way
// Z_FreeTags(PU_LEVEL) did), including any that were removed from the thinker list
// without being individually freed (Doom::unArchiveThinkers marks-and-empties). alloc
// zeroes, matching the first-load memory the demos recorded; free unlinks and
// releases one block (the lazy per-tic free and P_UnArchive's clear).
void* levelAlloc(int size);
void levelFree(void* block);
void freeLevelAllocations();
} // namespace Doom
