#pragma once

#include "../r_defs.h"

#include "Blockmap.h"

#include <ea_data_structures/Structures/Vector.h>

namespace Doom
{
// A level's static geometry: the arrays that are built once when the level loads
// and thrown away whole when the next one does. This is the PU_LEVEL half of the
// zone allocator's job - the clean half. The other half, mobjs and the thinker
// specials (doors, lifts, lights), have their own per-object lifecycle
// (P_RemoveThinker frees them mid-play) and stay on the zone until the playsim is
// rewritten.
//
// Vanilla loads these with Z_Malloc(..., PU_LEVEL) and frees them all at once with
// Z_FreeTags at the top of the next P_SetupLevel. A Level frees them by owning
// them: load a new level and the old vectors are replaced, which is the
// destructor doing what Z_FreeTags did, but scoped and automatic.
//
// The vanilla globals (vertexes, numsegs, sectors, ...) remain, as views onto
// these vectors - the renderer and the playsim index them thousands of times and
// are not being rewritten yet. resize() reallocates, so each loader refreshes its
// global pointer after filling its vector; within a level the vectors never move,
// so the cross-references the loaders build between them (a seg pointing at a
// vertex, a subsector at a sector) stay valid.
struct Level
{
    EA::Vector<vertex_t> vertexes;
    EA::Vector<seg_t> segs;
    EA::Vector<subsector_t> subsectors;
    EA::Vector<sector_t> sectors;
    EA::Vector<node_t> nodes;
    EA::Vector<line_t> lines;
    EA::Vector<side_t> sides;

    // The per-block mobj chain heads. The array is ours; the mobjs it points at
    // are the zone's.
    EA::Vector<mobj_t*> blockLinks;

    // The blockmap descriptor - origin, extent and the lump pointers the iterators
    // read from. Filled by P_LoadBlockMap, which then refreshes the vanilla
    // bmaporgx/bmapwidth/blockmap globals as views onto it. blockLinks above is the
    // one blockmap array that is ours to allocate; the lump itself is WadFile's.
    Blockmap blockmap;

    // One flat array of line pointers, carved into per-sector slices that
    // sector_t::lines point into. Vanilla calls this `linebuffer`, a single
    // Z_Malloc in P_GroupLines.
    EA::Vector<line_t*> sectorLines;
};

// The engine's one level, for as long as the engine has one of everything.
Level& level();
} // namespace Doom
