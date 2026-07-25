#pragma once

#include "ActionFunc.h" // Thinker
#include "ThinkerList.h"

namespace Doom
{
// Allocate a thinker and register it, in one step. This is what `levelAlloc` plus
// `addThinker` were: the block came from one intrusive list and the ordering came
// from another, and a caller that did the first without the second leaked until the
// next level load. The ThinkerList owns what it orders now, so there is no window
// between the two and no way to be in one and not the other.
//
// The thinker is live from here on - it is at the back of the list before its caller
// has finished filling it in, where it used to be appended after. Nothing walks the
// list between a spawner's allocation and its old addThinker call, so the resulting
// order is unchanged, which is what the demo goldens check.
template <typename T>
T& addThinker()
{
    return thinkerList().createDerived<T>();
}

void removeThinker(Thinker& thinker);
void runThinkers();
void ticker();
} // namespace Doom
