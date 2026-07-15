#pragma once

#include "../d_think.h" // thinker_t

namespace Doom
{
// The thinker list and the per-tic ticker; p_tick.cpp keeps the vanilla names.
void initThinkers(void);
void addThinker(thinker_t* thinker);
void removeThinker(thinker_t* thinker);
void runThinkers(void);
void ticker(void);
} // namespace Doom
