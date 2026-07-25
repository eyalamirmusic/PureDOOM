#pragma once

#include "../Containers.h"
#include "Thinker.h"

namespace Doom
{
// Every actor that acts once a tic - a mobj, a moving-sector special - in the order
// it was spawned. runThinkers walks it front to back; SaveGame, Enemy, Teleport,
// Render/Data and the eacp port walk the same order to find mobjs.
//
// This was vanilla's thinkercap: a circular doubly-linked list with a sentinel head,
// threaded through `prev`/`next` fields on Thinker itself, sitting *alongside* a
// second intrusive list (LevelPool) that owned the same blocks so a level reload
// could reclaim them. Both are gone. An OwnedVector is both lists at once - it holds
// the order and it holds the ownership - so allocation and registration are one step,
// a thinker cannot be orphaned by being dropped from one list and not the other, and
// the level reset is a single clear().
//
// It is a vector of *owning pointers*, which is the load-bearing part: the objects
// are individually allocated and never move, so every Mobj* held elsewhere (a
// target, a tracer, player->mo, a sector's soundtarget or specialdata, the
// activeplats/activeceilings slot tables, the blockmap and sector thing links) stays
// valid across an append. A Vector<Thinker> by value could not exist for that reason
// alone, quite apart from the type being polymorphic.
//
// Two constraints on anyone rewriting runThinkers, both of which the demo goldens
// enforce to the tic:
//   - iterate by *index*, re-reading size() each time. A thinker's tick() spawns
//     thinkers, which append here; vanilla reached those in the same tic, and an
//     index loop reproduces that exactly. A range-for or a cached iterator does not,
//     and the append reallocates the buffer out from under it.
//   - erase at the cursor with removeAt (an order-preserving erase), never
//     swap-and-pop. The order *is* the simulation.
using ThinkerList = OwnedVector<Thinker>;

// The one ThinkerList, a view onto the Engine's member - the same pattern as the
// other clusters.
ThinkerList& thinkerList();
} // namespace Doom
